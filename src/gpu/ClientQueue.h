#pragma once

// The part of a GPU client that's the same whatever it does: jobs queued from any thread and taken by the render thread,
// a few sets of frame buffers reused as frames complete, each job counted until it has called back, and every job owed
// handed to a fallback (the CPU) if the GPU fails. What finishes a job runs in the TBB arena the client was made in (a
// job graph's): not in the render thread's default arena, where it would wait for workers the graph keeps busy.

#include "ContextImpl.h"

#include <oneapi/tbb/task_arena.h>

#include <atomic>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace tesworld::gpu::detail
{
	// Frame is a client's frame set: `bool busy`, and `placed`, whose items each hold the `job` they do.
	template <class Job, class Frame>
	class ClientQueue
	{
	public:
		// a_fallback does a job elsewhere; it, or what it starts, calls Finished once the job has called back. a_inline runs
		// what Run is given at once, on the calling thread (the render thread's, for jobs finished there), instead of as a
		// TBB task: for a client whose callers block TBB threads waiting on their jobs, which a task might wait behind.
		ClientQueue(ContextImpl& a_gpu, std::size_t a_frameSets, std::function<void(Job)> a_fallback, bool a_inline = false) :
			_gpu(a_gpu),
			_frameSets(a_frameSets),
			_fallback(std::move(a_fallback)),
			_runInline(a_inline),
			_arena(oneapi::tbb::task_arena::attach{})
		{}

		~ClientQueue() { WaitTasks(); }

		// ---- Any thread ----

		void Push(Job a_job)
		{
			_outstanding.fetch_add(1, std::memory_order_acq_rel);
			{
				std::scoped_lock lock(_mutex);
				if (!_failed) {
					_queued.push_back(std::move(a_job));
					_gpu.Wake();
					return;
				}
			}
			_fallback(std::move(a_job));
		}

		// A job has called back.
		void Finished()
		{
			if (_outstanding.fetch_sub(1, std::memory_order_acq_rel) == 1) {
				_outstanding.notify_all();
			}
		}

		// Runs a_task as a TBB task in the client's arena, which the queue waits for.
		template <class Task>
		void Run(Task&& a_task)
		{
			if (_runInline) {
				a_task();
				return;
			}
			_tasks.fetch_add(1, std::memory_order_acq_rel);
			_arena.enqueue([this, task = std::forward<Task>(a_task)] {
				task();
				if (_tasks.fetch_sub(1, std::memory_order_acq_rel) == 1) _tasks.notify_all();
			});
		}

		// Returns once every job so far has called back.
		void Wait()
		{
			for (;;) {
				WaitTasks();
				const auto outstanding = _outstanding.load(std::memory_order_acquire);
				if (outstanding == 0) break;
				_outstanding.wait(outstanding, std::memory_order_acquire);
			}
			WaitTasks();
		}

		// Returns once every task run so far has.
		void WaitTasks()
		{
			for (;;) {
				const auto running = _tasks.load(std::memory_order_acquire);
				if (running == 0) return;
				_tasks.wait(running, std::memory_order_acquire);
			}
		}

		// Why the GPU failed, or nothing.
		[[nodiscard]] std::string Failure() const
		{
			std::scoped_lock lock(_mutex);
			return _failure;
		}

		[[nodiscard]] bool Failed() const
		{
			std::scoped_lock lock(_mutex);
			return _failed;
		}

		// ---- Render thread ----

		// The jobs not yet in a frame, oldest first, those queued since taken in.
		std::deque<Job>& Waiting()
		{
			std::scoped_lock lock(_mutex);
			_waiting.insert(_waiting.end(), std::make_move_iterator(_queued.begin()), std::make_move_iterator(_queued.end()));
			_queued.clear();
			return _waiting;
		}

		// A frame set not in flight, or none while every set is.
		Frame* AcquireFrame()
		{
			for (auto& frame : _frames) {
				if (!frame->busy) return frame.get();
			}
			if (_frames.size() < _frameSets) {
				_frames.push_back(std::make_unique<Frame>());
				return _frames.back().get();
			}
			return nullptr;
		}

		[[nodiscard]] const std::vector<std::unique_ptr<Frame>>& Frames() const { return _frames; }

		// Every job owed, queued or in a frame, to the fallback, each in a task; later jobs go straight there.
		void Fail(const std::string& a_reason)
		{
			std::deque<Job> owed;
			{
				std::scoped_lock lock(_mutex);
				_failed = true;
				_failure = a_reason;
				owed.insert(owed.end(), std::make_move_iterator(_queued.begin()), std::make_move_iterator(_queued.end()));
				_queued.clear();
			}
			owed.insert(owed.end(), std::make_move_iterator(_waiting.begin()), std::make_move_iterator(_waiting.end()));
			_waiting.clear();
			for (auto& frame : _frames) {
				for (auto& placed : frame->placed) owed.push_back(std::move(placed.job));
				frame->placed.clear();
				frame->busy = false;
			}
			for (auto& job : owed) {
				auto shared = std::make_shared<Job>(std::move(job));
				Run([this, shared] { _fallback(std::move(*shared)); });
			}
		}

	private:
		ContextImpl& _gpu;
		std::size_t _frameSets;
		std::function<void(Job)> _fallback;
		bool _runInline{ false };

		mutable std::mutex _mutex;
		std::deque<Job> _queued;
		bool _failed{ false };
		std::string _failure;
		std::atomic<std::size_t> _outstanding{ 0 };
		oneapi::tbb::task_arena _arena;
		std::atomic<std::size_t> _tasks{ 0 };

		// Render thread only.
		std::deque<Job> _waiting;
		std::vector<std::unique_ptr<Frame>> _frames;
	};
}