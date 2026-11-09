#include "tesworld/Plugins.h"
#include "tesworld/Platform.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

#include <oneapi/tbb/blocked_range.h>
#include "tesworld/Profiling.h"

#include <oneapi/tbb/enumerable_thread_specific.h>
#include <oneapi/tbb/parallel_for.h>
#include <zlib.h>

namespace tesworld
{
	namespace
	{
		using Clock = std::chrono::steady_clock;

		constexpr FourCC SIG_TES4 = MakeFourCC('T', 'E', 'S', '4');
		constexpr FourCC SIG_GRUP = MakeFourCC('G', 'R', 'U', 'P');
		constexpr FourCC SIG_XXXX = MakeFourCC('X', 'X', 'X', 'X');
		constexpr FourCC SIG_EDID = MakeFourCC('E', 'D', 'I', 'D');
		constexpr FourCC SIG_MAST = MakeFourCC('M', 'A', 'S', 'T');
		constexpr FourCC SIG_CNAM = MakeFourCC('C', 'N', 'A', 'M');
		constexpr FourCC SIG_SNAM = MakeFourCC('S', 'N', 'A', 'M');
		constexpr FourCC SIG_HEDR = MakeFourCC('H', 'E', 'D', 'R');
		constexpr FourCC SIG_ONAM = MakeFourCC('O', 'N', 'A', 'M');
		constexpr FourCC SIG_MODL = MakeFourCC('M', 'O', 'D', 'L');
		constexpr FourCC SIG_MOD2 = MakeFourCC('M', 'O', 'D', '2');
		constexpr FourCC SIG_MOD3 = MakeFourCC('M', 'O', 'D', '3');
		constexpr FourCC SIG_MOD4 = MakeFourCC('M', 'O', 'D', '4');
		constexpr FourCC SIG_MOD5 = MakeFourCC('M', 'O', 'D', '5');
		constexpr FourCC SIG_MODS = MakeFourCC('M', 'O', 'D', 'S');
		constexpr FourCC SIG_MO2S = MakeFourCC('M', 'O', '2', 'S');
		constexpr FourCC SIG_MO3S = MakeFourCC('M', 'O', '3', 'S');
		constexpr FourCC SIG_MO4S = MakeFourCC('M', 'O', '4', 'S');
		constexpr FourCC SIG_MO5S = MakeFourCC('M', 'O', '5', 'S');
		constexpr FourCC SIG_MNAM = MakeFourCC('M', 'N', 'A', 'M');
		constexpr FourCC SIG_NAME = MakeFourCC('N', 'A', 'M', 'E');
		constexpr FourCC SIG_DATA = MakeFourCC('D', 'A', 'T', 'A');
		constexpr FourCC SIG_XCLC = MakeFourCC('X', 'C', 'L', 'C');
		constexpr FourCC SIG_XCLW = MakeFourCC('X', 'C', 'L', 'W');
		constexpr FourCC SIG_XSCL = MakeFourCC('X', 'S', 'C', 'L');
		constexpr FourCC SIG_XESP = MakeFourCC('X', 'E', 'S', 'P');
		constexpr FourCC SIG_XEMI = MakeFourCC('X', 'E', 'M', 'I');
		constexpr FourCC SIG_REGN = MakeFourCC('R', 'E', 'G', 'N');
		constexpr FourCC SIG_STAT = MakeFourCC('S', 'T', 'A', 'T');
		constexpr FourCC SIG_MSTT = MakeFourCC('M', 'S', 'T', 'T');
		constexpr FourCC SIG_TREE = MakeFourCC('T', 'R', 'E', 'E');
		constexpr FourCC SIG_ACTI = MakeFourCC('A', 'C', 'T', 'I');
		constexpr FourCC SIG_DOOR = MakeFourCC('D', 'O', 'O', 'R');
		constexpr FourCC SIG_CONT = MakeFourCC('C', 'O', 'N', 'T');
		constexpr FourCC SIG_FURN = MakeFourCC('F', 'U', 'R', 'N');
		constexpr FourCC SIG_LIGH = MakeFourCC('L', 'I', 'G', 'H');
		constexpr FourCC SIG_WRLD = MakeFourCC('W', 'R', 'L', 'D');
		constexpr FourCC SIG_CELL = MakeFourCC('C', 'E', 'L', 'L');
		constexpr FourCC SIG_REFR = MakeFourCC('R', 'E', 'F', 'R');
		constexpr FourCC SIG_ACHR = MakeFourCC('A', 'C', 'H', 'R');
		constexpr FourCC SIG_TXST = MakeFourCC('T', 'X', 'S', 'T');
		constexpr FourCC SIG_LTEX = MakeFourCC('L', 'T', 'E', 'X');
		constexpr FourCC SIG_GRAS = MakeFourCC('G', 'R', 'A', 'S');
		constexpr FourCC SIG_LAND = MakeFourCC('L', 'A', 'N', 'D');
		constexpr FourCC SIG_TX00 = MakeFourCC('T', 'X', '0', '0');
		constexpr FourCC SIG_TX01 = MakeFourCC('T', 'X', '0', '1');
		constexpr FourCC SIG_TX02 = MakeFourCC('T', 'X', '0', '2');
		constexpr FourCC SIG_TX03 = MakeFourCC('T', 'X', '0', '3');
		constexpr FourCC SIG_TX04 = MakeFourCC('T', 'X', '0', '4');
		constexpr FourCC SIG_TX05 = MakeFourCC('T', 'X', '0', '5');
		constexpr FourCC SIG_TX06 = MakeFourCC('T', 'X', '0', '6');
		constexpr FourCC SIG_TX07 = MakeFourCC('T', 'X', '0', '7');
		constexpr FourCC SIG_DNAM = MakeFourCC('D', 'N', 'A', 'M');
		constexpr FourCC SIG_HNAM = MakeFourCC('H', 'N', 'A', 'M');
		constexpr FourCC SIG_GNAM = MakeFourCC('G', 'N', 'A', 'M');
		constexpr FourCC SIG_TNAM = MakeFourCC('T', 'N', 'A', 'M');
		constexpr FourCC SIG_INAM = MakeFourCC('I', 'N', 'A', 'M');
		constexpr FourCC SIG_VHGT = MakeFourCC('V', 'H', 'G', 'T');
		constexpr FourCC SIG_VNML = MakeFourCC('V', 'N', 'M', 'L');
		constexpr FourCC SIG_VCLR = MakeFourCC('V', 'C', 'L', 'R');
		constexpr FourCC SIG_BTXT = MakeFourCC('B', 'T', 'X', 'T');
		constexpr FourCC SIG_ATXT = MakeFourCC('A', 'T', 'X', 'T');
		constexpr FourCC SIG_VTXT = MakeFourCC('V', 'T', 'X', 'T');
		constexpr FourCC SIG_SCOL = MakeFourCC('S', 'C', 'O', 'L');
		constexpr FourCC SIG_FLOR = MakeFourCC('F', 'L', 'O', 'R');
		constexpr FourCC SIG_OBND = MakeFourCC('O', 'B', 'N', 'D');
		constexpr FourCC SIG_WNAM = MakeFourCC('W', 'N', 'A', 'M');
		constexpr FourCC SIG_PNAM = MakeFourCC('P', 'N', 'A', 'M');
		constexpr FourCC SIG_NAM0 = MakeFourCC('N', 'A', 'M', '0');
		constexpr FourCC SIG_NAM9 = MakeFourCC('N', 'A', 'M', '9');
		constexpr FourCC SIG_NAM3 = MakeFourCC('N', 'A', 'M', '3');
		constexpr FourCC SIG_NAM4 = MakeFourCC('N', 'A', 'M', '4');
		constexpr FourCC SIG_NAMA = MakeFourCC('N', 'A', 'M', 'A');
		constexpr FourCC SIG_RNAM = MakeFourCC('R', 'N', 'A', 'M');
		constexpr FourCC SIG_UNAM = MakeFourCC('U', 'N', 'A', 'M');
		constexpr FourCC SIG_MATO = MakeFourCC('M', 'A', 'T', 'O');
		constexpr FourCC SIG_MATT = MakeFourCC('M', 'A', 'T', 'T');
		constexpr std::uint32_t TES4_FLAG_ESM = 0x00000001u;

		constexpr std::uint32_t RECORD_FLAG_COMPRESSED = 0x00040000u;
		constexpr std::uint32_t TES4_FLAG_ESL = 0x00000200u;

		template <class T>
		[[nodiscard]] T ReadLE(std::span<const std::uint8_t> a_bytes, std::size_t a_offset)
		{
			if (a_offset + sizeof(T) > a_bytes.size()) {
				throw std::runtime_error("unexpected end of plugin data");
			}
			T value{};
			std::memcpy(std::addressof(value), a_bytes.data() + a_offset, sizeof(T));
			return value;
		}

		[[nodiscard]] bool IsValidCellWaterHeight(float a_value) noexcept
		{
			return std::isfinite(a_value) && std::abs(a_value) < 100000000.0f;
		}


		using platform::MapFileBytes;
		using platform::ReadFileBytes;


		[[nodiscard]] std::string ReadString(std::span<const std::uint8_t> a_data)
		{
			const auto* begin = reinterpret_cast<const char*>(a_data.data());
			const auto* end = begin + a_data.size();
			const auto* nullPos = std::find(begin, end, '\0');
			return std::string(begin, nullPos);
		}

		[[nodiscard]] FormID ResolveDefaultLandTexture(FormID a_landTextureFormID)
		{
			return a_landTextureFormID.IsNull() ? kSkyrimDefaultLandTextureFormID : a_landTextureFormID;
		}

		[[nodiscard]] std::string ReadString260(std::span<const std::uint8_t> a_data, std::size_t a_index)
		{
			const auto offset = a_index * 260u;
			if (offset >= a_data.size()) {
				return {};
			}
			const auto size = (std::min<std::size_t>)(260u, a_data.size() - offset);
			return ReadString(a_data.subspan(offset, size));
		}

		[[nodiscard]] bool IsModelTextureSwapSubrecord(FourCC a_signature)
		{
			return a_signature == SIG_MODS ||
			       a_signature == SIG_MO2S ||
			       a_signature == SIG_MO3S ||
			       a_signature == SIG_MO4S ||
			       a_signature == SIG_MO5S;
		}

		[[nodiscard]] std::optional<std::size_t> TextureSlotIndexForSubrecord(FourCC a_signature)
		{
			switch (a_signature) {
			case SIG_TX00:
				return 0;
			case SIG_TX01:
				return 1;
			case SIG_TX02:
				return 2;
			case SIG_TX03:
				return 3;
			case SIG_TX04:
				return 4;
			case SIG_TX05:
				return 5;
			case SIG_TX06:
				return 6;
			case SIG_TX07:
				return 7;
			default:
				return std::nullopt;
			}
		}

		[[nodiscard]] std::vector<std::uint8_t> InflateRecord(std::span<const std::uint8_t> a_recordData)
		{
			if (a_recordData.size() < sizeof(std::uint32_t)) {
				throw std::runtime_error("compressed record is missing uncompressed size");
			}
			const auto uncompressedSize = ReadLE<std::uint32_t>(a_recordData, 0);
			std::vector<std::uint8_t> output(uncompressedSize);
			uLongf outputSize = static_cast<uLongf>(output.size());
			const auto* compressed = reinterpret_cast<const Bytef*>(a_recordData.data() + sizeof(std::uint32_t));
			const auto compressedSize = static_cast<uLong>(a_recordData.size() - sizeof(std::uint32_t));
			const auto result = uncompress(reinterpret_cast<Bytef*>(output.data()), std::addressof(outputSize), compressed, compressedSize);
			if (result != Z_OK || outputSize != output.size()) {
				throw std::runtime_error("zlib failed to inflate compressed record");
			}
			return output;
		}

		void InflateRecordInto(std::span<const std::uint8_t> a_recordData, std::vector<std::uint8_t>& a_output)
		{
			if (a_recordData.size() < sizeof(std::uint32_t)) {
				throw std::runtime_error("compressed record is missing uncompressed size");
			}
			const auto uncompressedSize = ReadLE<std::uint32_t>(a_recordData, 0);
			a_output.resize(uncompressedSize);
			uLongf outputSize = static_cast<uLongf>(a_output.size());
			const auto* compressed = reinterpret_cast<const Bytef*>(a_recordData.data() + sizeof(std::uint32_t));
			const auto compressedSize = static_cast<uLong>(a_recordData.size() - sizeof(std::uint32_t));
			const auto result = uncompress(reinterpret_cast<Bytef*>(a_output.data()), std::addressof(outputSize), compressed, compressedSize);
			if (result != Z_OK || outputSize != a_output.size()) {
				throw std::runtime_error("zlib failed to inflate compressed record");
			}
		}

		[[nodiscard]] double ElapsedMilliseconds(Clock::time_point a_begin)
		{
			return std::chrono::duration<double, std::milli>(Clock::now() - a_begin).count();
		}

		[[nodiscard]] bool IsPluginExtension(std::string_view a_name)
		{
			const auto lower = LowerAscii(a_name);
			return lower.ends_with(".esm") || lower.ends_with(".esp") || lower.ends_with(".esl");
		}

		[[nodiscard]] std::string PluginNameFromPath(const std::filesystem::path& a_path)
		{
			return LowerAscii(platform::PathToUtf8(a_path.filename()));
		}

		[[nodiscard]] std::size_t FindLoadOrderIndex(std::span<const LoadOrderEntry> a_loadOrder, std::string_view a_pluginName)
		{
			const auto key = LowerAscii(a_pluginName);
			for (std::size_t i = 0; i < a_loadOrder.size(); ++i) {
				if (LowerAscii(a_loadOrder[i].pluginName) == key) {
					return i;
				}
			}
			throw std::runtime_error("missing master in load order: " + std::string(a_pluginName));
		}

		[[nodiscard]] std::shared_ptr<const std::vector<FileID>> BuildSourceMasterFileIDs(const LoadOrderEntry& a_entry, std::span<const LoadOrderEntry> a_loadOrder)
		{
			std::vector<FileID> result;
			result.reserve(a_entry.masters.size());
			for (const auto& master : a_entry.masters) {
				result.push_back(a_loadOrder[FindLoadOrderIndex(a_loadOrder, master)].fileID);
			}
			return std::make_shared<const std::vector<FileID>>(std::move(result));
		}

		[[nodiscard]] FormID ConvertRecordLocalFormID(FormID a_fileFormID, const RawRecord& a_record)
		{
			if (a_fileFormID.IsNull() || a_fileFormID.IsNone() || a_fileFormID.IsHardcoded()) {
				return a_fileFormID;
			}

			const auto rawFileSlot = static_cast<std::uint8_t>(a_fileFormID.value >> 24);
			auto owner = a_record.sourceFileID;
			if (a_record.sourceMasterFileIDs && rawFileSlot < a_record.sourceMasterFileIDs->size()) {
				owner = (*a_record.sourceMasterFileIDs)[rawFileSlot];
			}

			const auto objectID = owner.kind == ModuleKind::Light ? (a_fileFormID.value & 0xFFFu) : (a_fileFormID.value & 0xFFFFFFu);
			return FormID{ owner.BaseFormID() | objectID };
		}

		[[nodiscard]] FormID ConvertContextLocalFormID(
			FormID a_fileFormID,
			FileID a_sourceFileID,
			const std::shared_ptr<const std::vector<FileID>>& a_sourceMasterFileIDs)
		{
			if (a_fileFormID.IsNull() || a_fileFormID.IsNone() || a_fileFormID.IsHardcoded()) {
				return a_fileFormID;
			}

			const auto rawFileSlot = static_cast<std::uint8_t>(a_fileFormID.value >> 24);
			auto owner = a_sourceFileID;
			if (a_sourceMasterFileIDs && rawFileSlot < a_sourceMasterFileIDs->size()) {
				owner = (*a_sourceMasterFileIDs)[rawFileSlot];
			}

			const auto objectID = owner.kind == ModuleKind::Light ? (a_fileFormID.value & 0xFFFu) : (a_fileFormID.value & 0xFFFFFFu);
			return FormID{ owner.BaseFormID() | objectID };
		}

		struct RecordHeader
		{
			FourCC signature{ 0 };
			std::uint32_t dataSize{ 0 };
			std::uint32_t flags{ 0 };
			FormID formID{};
			std::uint32_t revision{ 0 };
			std::uint16_t formVersion{ 0 };
			std::uint16_t unknown{ 0 };
		};

		[[nodiscard]] RecordHeader ReadRecordHeader(std::span<const std::uint8_t> a_bytes, std::size_t a_offset)
		{
			RecordHeader header{};
			header.signature = ReadLE<FourCC>(a_bytes, a_offset + 0);
			header.dataSize = ReadLE<std::uint32_t>(a_bytes, a_offset + 4);
			header.flags = ReadLE<std::uint32_t>(a_bytes, a_offset + 8);
			header.formID = FormID{ ReadLE<std::uint32_t>(a_bytes, a_offset + 12) };
			header.revision = ReadLE<std::uint32_t>(a_bytes, a_offset + 16);
			header.formVersion = ReadLE<std::uint16_t>(a_bytes, a_offset + 20);
			header.unknown = ReadLE<std::uint16_t>(a_bytes, a_offset + 22);
			return header;
		}

		struct GroupHeader
		{
			std::uint32_t groupSize{ 0 };
			std::uint32_t label{ 0 };
			std::int32_t type{ 0 };
			std::int16_t gridY{ 0 };
			std::int16_t gridX{ 0 };
		};

		[[nodiscard]] GroupHeader ReadGroupHeader(std::span<const std::uint8_t> a_bytes, std::size_t a_offset)
		{
			GroupHeader header{};
			header.groupSize = ReadLE<std::uint32_t>(a_bytes, a_offset + 4);
			header.label = ReadLE<std::uint32_t>(a_bytes, a_offset + 8);
			header.type = ReadLE<std::int32_t>(a_bytes, a_offset + 12);
			header.gridY = ReadLE<std::int16_t>(a_bytes, a_offset + 8);
			header.gridX = ReadLE<std::int16_t>(a_bytes, a_offset + 10);
			return header;
		}

		[[nodiscard]] BaseObjectKind BaseKindForSignature(FourCC a_signature);
		[[nodiscard]] bool IsBaseObjectSignature(FourCC a_signature);

		[[nodiscard]] bool IsStaticWorldRecordSignature(FourCC a_signature)
		{
			return IsBaseObjectSignature(a_signature) ||
			       a_signature == SIG_TXST ||
			       a_signature == SIG_LTEX ||
			       a_signature == SIG_GRAS ||
			       a_signature == SIG_CELL ||
			       a_signature == SIG_LAND ||
			       a_signature == SIG_REFR ||
			       a_signature == SIG_ACHR;
		}

		[[nodiscard]] bool ShouldRetainRecord(FourCC a_recordSignature, PluginParser::ParseMode a_mode)
		{
			return a_mode == PluginParser::ParseMode::FullRaw || IsStaticWorldRecordSignature(a_recordSignature);
		}

		[[nodiscard]] bool ShouldRetainSubrecord(FourCC a_recordSignature, FourCC a_subrecordSignature, PluginParser::ParseMode a_mode)
		{
			if (a_mode == PluginParser::ParseMode::FullRaw || a_subrecordSignature == SIG_XXXX) {
				return true;
			}
			if (IsBaseObjectSignature(a_recordSignature)) {
				return a_subrecordSignature == SIG_EDID ||
				       a_subrecordSignature == SIG_OBND ||
				       a_subrecordSignature == SIG_DNAM ||
				       a_subrecordSignature == SIG_MODL ||
				       a_subrecordSignature == SIG_MNAM ||
				       IsModelTextureSwapSubrecord(a_subrecordSignature);
			}
			if (a_recordSignature == SIG_TXST) {
				return a_subrecordSignature == SIG_EDID ||
				       a_subrecordSignature == SIG_DNAM ||
				       TextureSlotIndexForSubrecord(a_subrecordSignature).has_value();
			}
			if (a_recordSignature == SIG_LTEX) {
				return a_subrecordSignature == SIG_EDID ||
				       a_subrecordSignature == SIG_HNAM ||
				       a_subrecordSignature == SIG_SNAM ||
				       a_subrecordSignature == SIG_GNAM ||
				       a_subrecordSignature == SIG_TNAM ||
				       a_subrecordSignature == SIG_MNAM ||
				       a_subrecordSignature == SIG_INAM;
			}
			if (a_recordSignature == SIG_CELL) {
				return a_subrecordSignature == SIG_EDID || a_subrecordSignature == SIG_DATA || a_subrecordSignature == SIG_XCLC ||
				       a_subrecordSignature == SIG_XCLW;
			}
			if (a_recordSignature == SIG_LAND) {
				return a_subrecordSignature == SIG_VHGT ||
				       a_subrecordSignature == SIG_VNML ||
				       a_subrecordSignature == SIG_VCLR ||
				       a_subrecordSignature == SIG_BTXT ||
				       a_subrecordSignature == SIG_ATXT ||
				       a_subrecordSignature == SIG_VTXT;
			}
			if (a_recordSignature == SIG_REFR || a_recordSignature == SIG_ACHR) {
				return a_subrecordSignature == SIG_NAME || a_subrecordSignature == SIG_DATA || a_subrecordSignature == SIG_XSCL || a_subrecordSignature == SIG_XESP || a_subrecordSignature == SIG_XEMI;
			}
			return false;
		}

		[[nodiscard]] std::vector<RawSubrecord> ParseSubrecords(
			std::span<const std::uint8_t> a_data,
			FourCC a_recordSignature = 0,
			PluginParser::ParseMode a_mode = PluginParser::ParseMode::FullRaw)
		{
			std::vector<RawSubrecord> subrecords;
			subrecords.reserve(8);
			std::size_t offset = 0;
			while (offset + 6 <= a_data.size()) {
				const auto signature = ReadLE<FourCC>(a_data, offset);
				const auto size16 = ReadLE<std::uint16_t>(a_data, offset + 4);
				offset += 6;

				std::uint32_t size = size16;
				if (signature == SIG_XXXX) {
					if (offset + 4 > a_data.size()) {
						break;
					}
					RawSubrecord xxxx{};
					xxxx.signature = signature;
					xxxx.size = 4;
					xxxx.dataOffset = static_cast<std::uint32_t>(offset);
					if (ShouldRetainSubrecord(a_recordSignature, xxxx.signature, a_mode)) {
						subrecords.push_back(std::move(xxxx));
					}
					size = ReadLE<std::uint32_t>(a_data, offset);
					offset += 4;
					if (offset + 6 > a_data.size()) {
						break;
					}
				}

				RawSubrecord subrecord{};
				subrecord.signature = signature == SIG_XXXX ? ReadLE<FourCC>(a_data, offset) : signature;
				if (signature == SIG_XXXX) {
					(void)ReadLE<std::uint16_t>(a_data, offset + 4);
					offset += 6;
				}
				subrecord.size = size;
				subrecord.dataOffset = static_cast<std::uint32_t>(offset);
				if (offset + size > a_data.size()) {
					break;
				}
				if (ShouldRetainSubrecord(a_recordSignature, subrecord.signature, a_mode)) {
					subrecords.push_back(std::move(subrecord));
				}
				offset += size;
			}
			return subrecords;
		}

		[[nodiscard]] std::span<const std::uint8_t> SubrecordData(std::span<const std::uint8_t> a_recordData, const RawSubrecord& a_subrecord)
		{
			if (a_subrecord.dataOffset > a_recordData.size() || a_subrecord.size > a_recordData.size() - a_subrecord.dataOffset) {
				return {};
			}
			return a_recordData.subspan(a_subrecord.dataOffset, a_subrecord.size);
		}

		[[nodiscard]] std::span<const std::uint8_t> SubrecordData(const RawRecord& a_record, const RawSubrecord& a_subrecord)
		{
			return SubrecordData(a_record.Data(), a_subrecord);
		}

		[[nodiscard]] const RawSubrecord* FindSubrecord(const RawRecord& a_record, FourCC a_signature)
		{
			for (const auto& subrecord : a_record.subrecords) {
				if (subrecord.signature == a_signature) {
					return std::addressof(subrecord);
				}
			}
			return nullptr;
		}

		class SubrecordCursor
		{
		public:
			explicit SubrecordCursor(std::span<const std::uint8_t> a_data) :
				_data(a_data)
			{}

			[[nodiscard]] bool Next(RawSubrecord& a_subrecord, std::span<const std::uint8_t>& a_payload)
			{
				if (_offset + 6 > _data.size()) {
					return false;
				}

				auto signature = ReadLE<FourCC>(_data, _offset);
				std::uint32_t size = ReadLE<std::uint16_t>(_data, _offset + 4);
				_offset += 6;

				if (signature == SIG_XXXX) {
					if (_offset + 4 > _data.size()) {
						_offset = _data.size();
						return false;
					}
					size = ReadLE<std::uint32_t>(_data, _offset);
					_offset += 4;
					if (_offset + 6 > _data.size()) {
						_offset = _data.size();
						return false;
					}
					signature = ReadLE<FourCC>(_data, _offset);
					_offset += 6;
				}

				if (_offset + size > _data.size()) {
					_offset = _data.size();
					return false;
				}

				a_subrecord.signature = signature;
				a_subrecord.size = size;
				a_subrecord.dataOffset = static_cast<std::uint32_t>(_offset);
				a_payload = _data.subspan(_offset, size);
				_offset += size;
				return true;
			}

		private:
			std::span<const std::uint8_t> _data;
			std::size_t _offset{ 0 };
		};

		void ParseHeaderRecordData(std::span<const std::uint8_t> a_data, PluginHeader& a_header)
		{
			const auto subrecords = ParseSubrecords(a_data);
			for (std::size_t i = 0; i < subrecords.size(); ++i) {
				const auto& subrecord = subrecords[i];
				const auto data = SubrecordData(a_data, subrecord);
				if (subrecord.signature == SIG_HEDR && data.size() >= 12) {
					a_header.version = ReadLE<float>(data, 0);
					a_header.recordCount = ReadLE<std::uint32_t>(data, 4);
					a_header.nextObjectID = ReadLE<std::uint32_t>(data, 8);
				} else if (subrecord.signature == SIG_CNAM) {
					a_header.author = ReadString(data);
				} else if (subrecord.signature == SIG_SNAM) {
					a_header.description = ReadString(data);
				} else if (subrecord.signature == SIG_MAST) {
					a_header.masters.push_back(ReadString(data));
				} else if (subrecord.signature == SIG_ONAM) {
					for (std::size_t offset = 0; offset + 4 <= data.size(); offset += 4) {
						a_header.onamOverrides.push_back(FormID{ ReadLE<std::uint32_t>(data, offset) });
					}
				}
			}
		}

		[[nodiscard]] ModuleKind DetectModuleKind(const std::filesystem::path& a_path, const PluginHeader& a_header)
		{
			const auto extension = LowerAscii(platform::PathToUtf8(a_path.extension()));
			if (extension == ".esl" || (a_header.flags & TES4_FLAG_ESL) != 0) {
				return ModuleKind::Light;
			}
			return ModuleKind::Full;
		}

		struct InflateStream
		{
			z_stream stream{};
			bool initialized{ false };

			InflateStream() = default;
			InflateStream(const InflateStream&) = delete;
			InflateStream& operator=(const InflateStream&) = delete;

			~InflateStream()
			{
				if (initialized) {
					inflateEnd(std::addressof(stream));
				}
			}

			void Reset(std::span<const std::uint8_t> a_compressedData)
			{
				if (a_compressedData.size() < sizeof(std::uint32_t)) {
					throw std::runtime_error("compressed record is missing uncompressed size");
				}

				if (!initialized) {
					if (inflateInit(std::addressof(stream)) != Z_OK) {
						throw std::runtime_error("zlib failed to initialize streaming inflate");
					}
					initialized = true;
				} else if (inflateReset(std::addressof(stream)) != Z_OK) {
					throw std::runtime_error("zlib failed to reset streaming inflate");
				}

				stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(a_compressedData.data() + sizeof(std::uint32_t)));
				stream.avail_in = static_cast<uInt>(a_compressedData.size() - sizeof(std::uint32_t));
			}
		};

		struct ParserContext
		{
			const LoadOrderEntry& entry;
			std::span<const LoadOrderEntry> loadOrder;
			PluginParser::ParseMode mode{ PluginParser::ParseMode::FullRaw };
			std::size_t sourceFileIndex{ 0 };
			std::shared_ptr<const std::vector<FileID>> sourceMasterFileIDs;
			std::vector<GroupPathEntry> groupPath;
			std::optional<FormID> currentWorld;
			std::optional<FormID> currentCell;
			std::vector<std::uint8_t> inflateScratch;
			std::vector<std::uint8_t> cellScanScratch;
			InflateStream cellInflateStream;
		};

		void UpdateContextForGroup(ParserContext& a_context, const GroupHeader& a_group)
		{
			if (a_group.type != 1 && a_group.type != 6 && a_group.type != 8 && a_group.type != 9 && a_group.type != 10) {
				return;
			}

			const auto parentID = ConvertContextLocalFormID(FormID{ a_group.label }, a_context.entry.fileID, a_context.sourceMasterFileIDs);
			if (a_group.type == 1) {
				a_context.currentWorld = parentID;
			} else {
				a_context.currentCell = parentID;
			}
		}

		void ParseRecord(std::span<const std::uint8_t> a_bytes, std::size_t a_offset, PluginFile& a_plugin, ParserContext& a_context)
		{
			const auto header = ReadRecordHeader(a_bytes, a_offset);
			const auto retainRecord = ShouldRetainRecord(header.signature, a_context.mode);
			const auto needsContextID = header.signature == SIG_CELL || header.signature == SIG_WRLD;
			if (!retainRecord && !needsContextID) {
				return;
			}

			const auto loadOrderFormID = ConvertContextLocalFormID(header.formID, a_context.entry.fileID, a_context.sourceMasterFileIDs);
			if (!retainRecord) {
				if (header.signature == SIG_WRLD) {
					a_context.currentWorld = loadOrderFormID;
				} else if (header.signature == SIG_CELL) {
					a_context.currentCell = loadOrderFormID;
				}
				return;
			}

			const auto metadataOnly =
				a_context.mode == PluginParser::ParseMode::StaticWorldIndex &&
				((header.flags & ((1u << 5) | (1u << 12))) != 0);

			std::span<const std::uint8_t> recordData = a_bytes.subspan(a_offset + 24, header.dataSize);
			std::vector<std::uint8_t> decompressed;
			if (!metadataOnly && (header.flags & RECORD_FLAG_COMPRESSED) != 0) {
				decompressed = InflateRecord(recordData);
				recordData = decompressed;
			}

			RawRecord record{};
			record.signature = header.signature;
			record.dataSize = metadataOnly ? 0u : static_cast<std::uint32_t>(recordData.size());
			record.flags = header.flags;
			record.fileFormID = header.formID;
			record.loadOrderFormID = loadOrderFormID;
			record.revision = header.revision;
			record.formVersion = header.formVersion;
			record.unknown = header.unknown;
			record.sourceFileIndex = a_context.sourceFileIndex;
			record.sourceFileID = a_context.entry.fileID;
			record.sourceMasterFileIDs = a_context.sourceMasterFileIDs;
			record.rawOffset = static_cast<std::uint64_t>(a_offset);
			if (a_context.mode == PluginParser::ParseMode::FullRaw) {
				record.groupPath = a_context.groupPath;
			}
			record.parentWorld = a_context.currentWorld;
			record.parentCell = a_context.currentCell;
			if (metadataOnly) {
				record.dataPtr = nullptr;
			} else if (decompressed.empty()) {
				record.dataPtr = recordData.data();
			} else {
				record.ownedData = std::move(decompressed);
				record.dataPtr = record.ownedData.data();
			}
			if (!metadataOnly) {
				record.subrecords = ParseSubrecords(record.Data(), record.signature, a_context.mode);
			}

			if (record.signature == SIG_CELL) {
				record.parentWorld = a_context.currentWorld;
				a_context.currentCell = record.loadOrderFormID;
			}

			if (record.signature == SIG_WRLD) {
				a_context.currentWorld = record.loadOrderFormID;
			}

			const auto index = a_plugin.records.size();
			a_plugin.records.push_back(std::move(record));
			if (a_context.mode == PluginParser::ParseMode::FullRaw) {
				a_plugin.recordsBySignature[header.signature].push_back(index);
			}
		}

		void ParseRange(std::span<const std::uint8_t> a_bytes, std::size_t a_begin, std::size_t a_end, PluginFile& a_plugin, ParserContext& a_context)
		{
			std::size_t offset = a_begin;
			while (offset + 24 <= a_end) {
				const auto signature = ReadLE<FourCC>(a_bytes, offset);
				if (signature == SIG_GRUP) {
					const auto group = ReadGroupHeader(a_bytes, offset);
					if (group.groupSize < 24 || offset + group.groupSize > a_end) {
						throw std::runtime_error("invalid GRUP size in " + a_plugin.entry.pluginName);
					}

					GroupPathEntry pathEntry{};
					pathEntry.type = group.type;
					pathEntry.label = group.label;
					pathEntry.gridX = group.gridX;
					pathEntry.gridY = group.gridY;
					a_context.groupPath.push_back(pathEntry);

					const auto savedWorld = a_context.currentWorld;
					const auto savedCell = a_context.currentCell;
					UpdateContextForGroup(a_context, group);
					ParseRange(a_bytes, offset + 24, offset + group.groupSize, a_plugin, a_context);
					a_context.currentWorld = savedWorld;
					a_context.currentCell = savedCell;
					a_context.groupPath.pop_back();
					offset += group.groupSize;
				} else {
					const auto header = ReadRecordHeader(a_bytes, offset);
					if (offset + 24 + header.dataSize > a_end) {
						throw std::runtime_error("invalid record size in " + a_plugin.entry.pluginName);
					}
					ParseRecord(a_bytes, offset, a_plugin, a_context);
					offset += 24 + header.dataSize;
				}
			}
		}

		[[nodiscard]] BaseObjectKind BaseKindForSignature(FourCC a_signature)
		{
			if (a_signature == SIG_STAT) {
				return BaseObjectKind::Static;
			}
			if (a_signature == SIG_MSTT) {
				return BaseObjectKind::MovableStatic;
			}
			if (a_signature == SIG_TREE) {
				return BaseObjectKind::Tree;
			}
			if (a_signature == SIG_ACTI) {
				return BaseObjectKind::Activator;
			}
			if (a_signature == SIG_DOOR) {
				return BaseObjectKind::Door;
			}
			if (a_signature == SIG_CONT) {
				return BaseObjectKind::Container;
			}
			if (a_signature == SIG_FURN) {
				return BaseObjectKind::Furniture;
			}
			if (a_signature == SIG_LIGH) {
				return BaseObjectKind::Light;
			}
			if (a_signature == SIG_SCOL) {
				return BaseObjectKind::StaticCollection;
			}
			if (a_signature == MakeFourCC('T', 'A', 'C', 'T')) {
				return BaseObjectKind::TalkingActivator;
			}
			if (a_signature == SIG_FLOR) {
				return BaseObjectKind::Flora;
			}
			return BaseObjectKind::Unknown;
		}

		[[nodiscard]] bool IsBaseObjectSignature(FourCC a_signature)
		{
			return BaseKindForSignature(a_signature) != BaseObjectKind::Unknown;
		}

		void ParseAlternateTextures(
			std::vector<AlternateTextureOverride>& a_output,
			FourCC a_subrecordSignature,
			std::span<const std::uint8_t> a_payload,
			const RawRecord* a_record = nullptr,
			FileID a_sourceFileID = {},
			const std::shared_ptr<const std::vector<FileID>>& a_sourceMasterFileIDs = {})
		{
			if (a_payload.size() < 4) {
				return;
			}

			const auto count = ReadLE<std::uint32_t>(a_payload, 0);
			std::size_t offset = 4;
			for (std::uint32_t i = 0; i < count && offset + 4 <= a_payload.size(); ++i) {
				const auto nameSize = ReadLE<std::uint32_t>(a_payload, offset);
				offset += 4;
				if (nameSize > a_payload.size() - offset || a_payload.size() - offset - nameSize < 8) {
					break;
				}

				std::string name{
					reinterpret_cast<const char*>(a_payload.data() + offset),
					reinterpret_cast<const char*>(a_payload.data() + offset + nameSize)
				};
				while (!name.empty() && name.back() == '\0') {
					name.pop_back();
				}
				offset += nameSize;

				const FormID textureSetFileID{ ReadLE<std::uint32_t>(a_payload, offset) };
				offset += 4;
				const auto index3D = ReadLE<std::uint32_t>(a_payload, offset);
				offset += 4;

				FormID textureSetFormID{};
				if (a_record) {
					textureSetFormID = ConvertRecordLocalFormID(textureSetFileID, *a_record);
				} else {
					textureSetFormID = ConvertContextLocalFormID(textureSetFileID, a_sourceFileID, a_sourceMasterFileIDs);
				}

				a_output.push_back(AlternateTextureOverride{
					.subrecordSignature = a_subrecordSignature,
					.name3D = std::move(name),
					.textureSetFormID = textureSetFormID,
					.index3D = index3D
				});
			}
		}

		[[nodiscard]] ObjectBounds ReadObjectBounds(std::span<const std::uint8_t> a_payload)
		{
			ObjectBounds bounds{};
			for (std::size_t i = 0; i < 3; ++i) {
				bounds.min[i] = ReadLE<std::int16_t>(a_payload, i * 2);
				bounds.max[i] = ReadLE<std::int16_t>(a_payload, 6 + i * 2);
			}
			return bounds;
		}

		// A static collection's DATA after an ONAM: its part's placements, 28 bytes each (position, rotation, scale).
		void ReadCollectionPlacements(CollectionPart& a_part, std::span<const std::uint8_t> a_payload)
		{
			for (std::size_t offset = 0; offset + 28 <= a_payload.size(); offset += 28) {
				CollectionPlacement placement{};
				for (std::size_t i = 0; i < 3; ++i) {
					placement.position[i] = ReadLE<float>(a_payload, offset + i * 4);
					placement.rotation[i] = ReadLE<float>(a_payload, offset + 12 + i * 4);
				}
				placement.scale = ReadLE<float>(a_payload, offset + 24);
				a_part.placements.push_back(placement);
			}
		}

		[[nodiscard]] std::optional<BaseObjectInfo> ExtractBaseObject(const RawRecord& a_record)
		{
			if (!IsBaseObjectSignature(a_record.signature) || a_record.IsDeleted() || a_record.IsIgnored()) {
				return std::nullopt;
			}

			BaseObjectInfo info{};
			info.formID = a_record.loadOrderFormID;
			info.signature = a_record.signature;
			info.kind = BaseKindForSignature(a_record.signature);
			info.flags = a_record.flags;

			if (const auto* edid = FindSubrecord(a_record, SIG_EDID)) {
				info.editorID = ReadString(SubrecordData(a_record, *edid));
			}
			if (const auto* model = FindSubrecord(a_record, SIG_MODL)) {
				info.modelPath = ReadString(SubrecordData(a_record, *model));
			}
			if (const auto* obnd = FindSubrecord(a_record, SIG_OBND); obnd && obnd->size >= 12) {
				info.bounds = ReadObjectBounds(SubrecordData(a_record, *obnd));
			}
			if (a_record.signature == SIG_STAT) {
				if (const auto* dnam = FindSubrecord(a_record, SIG_DNAM); dnam && dnam->size >= 8) {
					info.material = ConvertRecordLocalFormID(FormID{ ReadLE<std::uint32_t>(SubrecordData(a_record, *dnam), 4) }, a_record);
				}
				if (const auto* lod = FindSubrecord(a_record, SIG_MNAM)) {
					const auto lodData = SubrecordData(a_record, *lod);
					for (std::size_t i = 0; i < info.lodModels.size(); ++i) {
						info.lodModels[i] = ReadString260(lodData, i);
					}
				}
			}
			for (const auto& subrecord : a_record.subrecords) {
				if (a_record.signature == SIG_SCOL && subrecord.signature == SIG_ONAM && subrecord.size >= 4) {
					info.parts.push_back(CollectionPart{
						.baseFormID = ConvertRecordLocalFormID(FormID{ ReadLE<std::uint32_t>(SubrecordData(a_record, subrecord), 0) }, a_record) });
				} else if (a_record.signature == SIG_SCOL && subrecord.signature == SIG_DATA && !info.parts.empty()) {
					ReadCollectionPlacements(info.parts.back(), SubrecordData(a_record, subrecord));
				} else if (IsModelTextureSwapSubrecord(subrecord.signature)) {
					ParseAlternateTextures(
						info.alternateTextures,
						subrecord.signature,
						SubrecordData(a_record, subrecord),
						std::addressof(a_record));
				}
			}
			return info;
		}

		[[nodiscard]] std::optional<TextureSetInfo> ExtractTextureSet(const RawRecord& a_record)
		{
			if (a_record.signature != SIG_TXST || a_record.IsDeleted() || a_record.IsIgnored()) {
				return std::nullopt;
			}

			TextureSetInfo info{};
			info.formID = a_record.loadOrderFormID;
			info.flags = a_record.flags;
			if (const auto* edid = FindSubrecord(a_record, SIG_EDID)) {
				info.editorID = ReadString(SubrecordData(a_record, *edid));
			}
			for (const auto& subrecord : a_record.subrecords) {
				if (const auto slot = TextureSlotIndexForSubrecord(subrecord.signature)) {
					info.texturePaths[*slot] = ReadString(SubrecordData(a_record, subrecord));
				} else if (subrecord.signature == SIG_DNAM && subrecord.size >= 2) {
					info.dnamFlags = ReadLE<std::uint16_t>(SubrecordData(a_record, subrecord), 0);
				}
			}
			return info;
		}

		[[nodiscard]] std::array<float, LandInfo::VertexCount> DecodeLandHeightsInternal(std::span<const std::uint8_t> a_vhgtPayload)
		{
			if (a_vhgtPayload.size() < sizeof(float) + LandInfo::VertexCount) {
				throw std::runtime_error("LAND VHGT payload is too small");
			}

			std::array<float, LandInfo::VertexCount> heights{};
			float rowBase = ReadLE<float>(a_vhgtPayload, 0);
			std::size_t gradientOffset = sizeof(float);
			for (std::size_t y = 0; y < LandInfo::VertexSide; ++y) {
				rowBase += static_cast<float>(static_cast<std::int8_t>(a_vhgtPayload[gradientOffset++]));
				float height = rowBase;
				heights[y * LandInfo::VertexSide] = height * 8.0f;
				for (std::size_t x = 1; x < LandInfo::VertexSide; ++x) {
					height += static_cast<float>(static_cast<std::int8_t>(a_vhgtPayload[gradientOffset++]));
					heights[y * LandInfo::VertexSide + x] = height * 8.0f;
				}
			}
			return heights;
		}

		[[nodiscard]] LandTextureInfo ExtractLandTextureFromData(
			const RecordHeader& a_header,
			FormID a_formID,
			FileID a_sourceFileID,
			const std::shared_ptr<const std::vector<FileID>>& a_sourceMasterFileIDs,
			std::span<const std::uint8_t> a_data)
		{
			LandTextureInfo info{};
			info.formID = a_formID;
			info.flags = a_header.flags;

			SubrecordCursor cursor(a_data);
			RawSubrecord subrecord{};
			std::span<const std::uint8_t> payload;
			while (cursor.Next(subrecord, payload)) {
				if (subrecord.signature == SIG_EDID) {
					info.editorID = ReadString(payload);
				} else if (subrecord.signature == SIG_HNAM && payload.size() >= 2) {
					info.havokFriction = payload[payload.size() >= 3 ? 1 : 0];
					info.havokRestitution = payload[payload.size() >= 3 ? 2 : 1];
				} else if (subrecord.signature == SIG_SNAM && payload.size() >= 1) {
					info.textureSpecular = payload[0];
				} else if (subrecord.signature == SIG_GNAM && payload.size() >= 4) {
					info.grassFormIDs.push_back(ConvertContextLocalFormID(
						FormID{ ReadLE<std::uint32_t>(payload, 0) },
						a_sourceFileID,
						a_sourceMasterFileIDs));
				} else if (subrecord.signature == SIG_TNAM && payload.size() >= 4) {
					info.textureSetFormID = ConvertContextLocalFormID(
						FormID{ ReadLE<std::uint32_t>(payload, 0) },
						a_sourceFileID,
						a_sourceMasterFileIDs);
				} else if (subrecord.signature == SIG_MNAM && payload.size() >= 4) {
					info.materialFormID = ConvertContextLocalFormID(
						FormID{ ReadLE<std::uint32_t>(payload, 0) },
						a_sourceFileID,
						a_sourceMasterFileIDs);
				} else if (subrecord.signature == SIG_INAM && payload.size() >= 4) {
					info.materialFlags = ReadLE<std::uint32_t>(payload, 0);
				}
			}
			return info;
		}

		[[nodiscard]] std::optional<LandTextureInfo> ExtractLandTexture(const RawRecord& a_record)
		{
			if (a_record.signature != SIG_LTEX || a_record.IsDeleted() || a_record.IsIgnored()) {
				return std::nullopt;
			}

			return ExtractLandTextureFromData(
				RecordHeader{
					.signature = a_record.signature,
					.dataSize = a_record.dataSize,
					.flags = a_record.flags,
					.formID = a_record.fileFormID,
					.revision = a_record.revision,
					.formVersion = a_record.formVersion,
					.unknown = a_record.unknown,
				},
				a_record.loadOrderFormID,
				a_record.sourceFileID,
				a_record.sourceMasterFileIDs,
				a_record.Data());
		}

		[[nodiscard]] GrassInfo ExtractGrassFromData(
			const RecordHeader& a_header,
			FormID a_formID,
			std::span<const std::uint8_t> a_data)
		{
			GrassInfo info{};
			info.formID = a_formID;
			info.flags = a_header.flags;

			SubrecordCursor cursor(a_data);
			RawSubrecord subrecord{};
			std::span<const std::uint8_t> payload;
			while (cursor.Next(subrecord, payload)) {
				if (subrecord.signature == SIG_EDID) {
					info.editorID = ReadString(payload);
				} else if (subrecord.signature == SIG_MODL) {
					info.modelPath = ReadString(payload);
				} else if (subrecord.signature == SIG_OBND && payload.size() >= 12) {
                    ObjectBounds bounds{};
                    for (std::size_t i = 0; i < 3; ++i) {
                        bounds.min[i] = ReadLE<std::int16_t>(payload, i * 2);
                        bounds.max[i] = ReadLE<std::int16_t>(payload, 6 + i * 2);
                    }
                    info.bounds = bounds;
                } else if (subrecord.signature == SIG_DATA && payload.size() >= 0x20) {
					info.density = ReadLE<std::uint8_t>(payload, 0);
					info.minSlopeDegrees = ReadLE<std::uint8_t>(payload, 1);
					info.maxSlopeDegrees = ReadLE<std::uint8_t>(payload, 2);
					info.distanceFromWaterLevel = ReadLE<std::uint16_t>(payload, 4);
					info.underwaterState = static_cast<GrassWaterState>(ReadLE<std::uint32_t>(payload, 8));
					info.positionRange = ReadLE<float>(payload, 0x0C);
					info.heightRange = ReadLE<float>(payload, 0x10);
					info.colorRange = ReadLE<float>(payload, 0x14);
					info.wavePeriod = ReadLE<float>(payload, 0x18);
					info.grassFlags = ReadLE<std::uint8_t>(payload, 0x1C);
				}
			}
			return info;
		}

		[[nodiscard]] std::optional<GrassInfo> ExtractGrass(const RawRecord& a_record)
		{
			if (a_record.signature != SIG_GRAS || a_record.IsDeleted() || a_record.IsIgnored()) {
				return std::nullopt;
			}
			return ExtractGrassFromData(
				RecordHeader{
					.signature = a_record.signature,
					.dataSize = a_record.dataSize,
					.flags = a_record.flags,
					.formID = a_record.fileFormID,
					.revision = a_record.revision,
					.formVersion = a_record.formVersion,
					.unknown = a_record.unknown,
				},
				a_record.loadOrderFormID,
				a_record.Data());
		}

		[[nodiscard]] std::optional<CellInfo> ExtractCell(const RawRecord& a_record)
		{
			if (a_record.signature != SIG_CELL || a_record.IsDeleted() || a_record.IsIgnored()) {
				return std::nullopt;
			}

			CellInfo info{};
			info.formID = a_record.loadOrderFormID;
			info.worldFormID = a_record.parentWorld;
			info.flags = a_record.flags;
			if (const auto* edid = FindSubrecord(a_record, SIG_EDID)) {
				info.editorID = ReadString(SubrecordData(a_record, *edid));
				info.fields |= CellInfo::FieldEditorID;
			}
			if (const auto* data = FindSubrecord(a_record, SIG_DATA)) {
				const auto bytes = SubrecordData(a_record, *data);
				if (bytes.size() == 1) {
					info.cellFlags = bytes[0];
					info.fields |= CellInfo::FieldData;
				} else if (bytes.size() >= 2) {
					info.cellFlags = ReadLE<std::uint16_t>(bytes, 0);
					info.fields |= CellInfo::FieldData;
				}
			}
			if (const auto* xclc = FindSubrecord(a_record, SIG_XCLC); xclc && xclc->size >= 8) {
				const auto bytes = SubrecordData(a_record, *xclc);
				info.gridX = ReadLE<std::int32_t>(bytes, 0);
				info.gridY = ReadLE<std::int32_t>(bytes, 4);
				info.forceHideLand = bytes.size() >= 12 ? ReadLE<std::uint32_t>(bytes, 8) : 0;
				info.fields |= CellInfo::FieldGrid;
			}
			if (const auto* xclw = FindSubrecord(a_record, SIG_XCLW); xclw && xclw->size >= 4) {
				const auto waterHeight = ReadLE<float>(SubrecordData(a_record, *xclw), 0);
				if (IsValidCellWaterHeight(waterHeight)) {
					info.waterHeight = waterHeight;
				}
				info.fields |= CellInfo::FieldWater;
			}
			return info;
		}

		[[nodiscard]] LandInfo ExtractLandFromData(
			const RecordHeader& a_header,
			FormID a_formID,
			std::optional<FormID> a_parentWorld,
			std::optional<FormID> a_parentCell,
			FileID a_sourceFileID,
			const std::shared_ptr<const std::vector<FileID>>& a_sourceMasterFileIDs,
			std::span<const std::uint8_t> a_data,
            bool a_engineGrass = false)
		{
			LandInfo info{};
			info.formID = a_formID;
			info.flags = a_header.flags;
			info.parentCell = a_parentCell.value_or(FormID{});
			info.worldFormID = a_parentWorld;
			for (auto& color : info.vertexColors) {
				color = { 255, 255, 255 };
			}

			std::uint32_t dataFlags = 0;
			std::optional<std::uint16_t> currentLayerIndex;
			std::optional<std::uint8_t> currentQuadrant;
			SubrecordCursor cursor(a_data);
			RawSubrecord subrecord{};
			std::span<const std::uint8_t> payload;
			while (cursor.Next(subrecord, payload)) {
				if (subrecord.signature == SIG_DATA && payload.size() >= 4) {
                    dataFlags = ReadLE<std::uint32_t>(payload, 0);
                } else if ((!a_engineGrass || (dataFlags & 1u)) && subrecord.signature == SIG_VHGT && payload.size() >= sizeof(float) + LandInfo::VertexCount) {
					info.heights = DecodeLandHeightsInternal(payload);
					info.hasHeights = true;
				} else if (subrecord.signature == SIG_VNML && payload.size() >= LandInfo::VertexCount * 3) {
					for (std::size_t i = 0; i < LandInfo::VertexCount; ++i) {
						info.sourceNormals[i] = {
							static_cast<std::int8_t>(payload[i * 3 + 0]),
							static_cast<std::int8_t>(payload[i * 3 + 1]),
							static_cast<std::int8_t>(payload[i * 3 + 2])
						};
					}
					info.hasSourceNormals = true;
				} else if ((!a_engineGrass || (dataFlags & 2u)) && subrecord.signature == SIG_VCLR && payload.size() >= LandInfo::VertexCount * 3) {
					for (std::size_t i = 0; i < LandInfo::VertexCount; ++i) {
						info.vertexColors[i] = {
							payload[i * 3 + 0],
							payload[i * 3 + 1],
							payload[i * 3 + 2]
						};
					}
					info.hasVertexColors = true;
				} else if (subrecord.signature == SIG_BTXT && payload.size() >= 8) {
					const auto quadrant = payload[4];
					if (quadrant < LandInfo::QuadrantCount) {
						info.baseTextures[quadrant] = LandBaseTexture{
							.landTextureFormID = ResolveDefaultLandTexture(ConvertContextLocalFormID(
								FormID{ ReadLE<std::uint32_t>(payload, 0) },
								a_sourceFileID,
								a_sourceMasterFileIDs)),
							.quadrant = quadrant,
						};
					}
					currentLayerIndex.reset();
					currentQuadrant.reset();
				} else if (subrecord.signature == SIG_ATXT && payload.size() >= 8) {
					const auto quadrant = payload[4];
					const auto layerIndex = ReadLE<std::uint16_t>(payload, 6);
					info.alphaTextures.push_back(LandAlphaTexture{
						.landTextureFormID = ResolveDefaultLandTexture(ConvertContextLocalFormID(
							FormID{ ReadLE<std::uint32_t>(payload, 0) },
							a_sourceFileID,
							a_sourceMasterFileIDs)),
						.quadrant = quadrant,
						.layerIndex = layerIndex,
					});
					currentQuadrant = quadrant;
					currentLayerIndex = layerIndex;
				} else if (subrecord.signature == SIG_VTXT && currentLayerIndex && currentQuadrant) {
					for (std::size_t offset = 0; offset + 8 <= payload.size(); offset += 8) {
						info.vertexAlphas.push_back(LandVertexAlpha{
							.quadrant = *currentQuadrant,
							.layerIndex = *currentLayerIndex,
							.position = ReadLE<std::uint16_t>(payload, offset + 0),
							.opacity = ReadLE<float>(payload, offset + 4),
						});
					}
				}
			}
			return info;
		}

		[[nodiscard]] std::optional<LandInfo> ExtractLand(const RawRecord& a_record)
		{
			if (a_record.signature != SIG_LAND || a_record.IsDeleted() || a_record.IsIgnored()) {
				return std::nullopt;
			}

			return ExtractLandFromData(
				RecordHeader{
					.signature = a_record.signature,
					.dataSize = a_record.dataSize,
					.flags = a_record.flags,
					.formID = a_record.fileFormID,
					.revision = a_record.revision,
					.formVersion = a_record.formVersion,
					.unknown = a_record.unknown,
				},
				a_record.loadOrderFormID,
				a_record.parentWorld,
				a_record.parentCell,
				a_record.sourceFileID,
				a_record.sourceMasterFileIDs,
				a_record.Data());
		}

		[[nodiscard]] std::optional<PlacementInfo> ExtractPlacement(const RawRecord& a_record)
		{
			if ((a_record.signature != SIG_REFR && a_record.signature != SIG_ACHR) || a_record.IsDeleted() || a_record.IsIgnored()) {
				return std::nullopt;
			}

			const auto* base = FindSubrecord(a_record, SIG_NAME);
			if (!base || base->size < 4) {
				return std::nullopt;
			}
			const auto baseData = SubrecordData(a_record, *base);

			PlacementInfo info{};
			info.formID = a_record.loadOrderFormID;
			info.parentCell = a_record.parentCell.value_or(FormID{});
			info.worldFormID = a_record.parentWorld;
			info.signature = a_record.signature;
			info.flags = a_record.flags;
			info.baseFormID = ConvertRecordLocalFormID(FormID{ ReadLE<std::uint32_t>(baseData, 0) }, a_record);
			info.sourceFile = a_record.sourceFileID;
			info.formVersion = a_record.formVersion;

			if (const auto* data = FindSubrecord(a_record, SIG_DATA); data && data->size >= 24) {
				const auto bytes = SubrecordData(a_record, *data);
				for (std::size_t i = 0; i < 3; ++i) {
					info.position[i] = ReadLE<float>(bytes, i * 4);
					info.rotation[i] = ReadLE<float>(bytes, 12 + i * 4);
				}
			}
			if (const auto* scale = FindSubrecord(a_record, SIG_XSCL); scale && scale->size >= 4) {
				info.scale = ReadLE<float>(SubrecordData(a_record, *scale), 0);
			}
			if (const auto* xesp = FindSubrecord(a_record, SIG_XESP); xesp && xesp->size >= 8) {
				const auto bytes = SubrecordData(a_record, *xesp);
				info.enableParent = ConvertRecordLocalFormID(FormID{ ReadLE<std::uint32_t>(bytes, 0) }, a_record);
				info.enableParentFlags = ReadLE<std::uint32_t>(bytes, 4);
			}
			if (const auto* xemi = FindSubrecord(a_record, SIG_XEMI); xemi && xemi->size >= 4)
				info.emittance = ConvertRecordLocalFormID(FormID{ReadLE<std::uint32_t>(SubrecordData(a_record, *xemi), 0)}, a_record);
			return info;
		}

		[[nodiscard]] bool IsSuppressedFlags(std::uint32_t a_flags)
		{
			return (a_flags & ((1u << 5) | (1u << 12))) != 0;
		}

		[[nodiscard]] WorldInfo ExtractWorldFromData(
			const RecordHeader& a_header,
			FormID a_formID,
			FileID a_sourceFileID,
			const std::shared_ptr<const std::vector<FileID>>& a_sourceMasterFileIDs,
			std::span<const std::uint8_t> a_data)
		{
			WorldInfo info{};
			info.formID = a_formID;
			info.flags = a_header.flags;
			SubrecordCursor cursor(a_data);
			RawSubrecord subrecord{};
			std::span<const std::uint8_t> payload;
			const auto toLoadOrder = [&](std::span<const std::uint8_t> a_bytes) {
				return ConvertContextLocalFormID(FormID{ ReadLE<std::uint32_t>(a_bytes, 0) }, a_sourceFileID, a_sourceMasterFileIDs);
			};
			while (cursor.Next(subrecord, payload)) {
				if (subrecord.signature == SIG_EDID) {
					info.editorID = ReadString(payload);
					info.fields |= WorldInfo::FieldEditorID;
				} else if (subrecord.signature == SIG_WNAM && payload.size() >= 4) {
					info.parentWorld = toLoadOrder(payload);
					info.fields |= WorldInfo::FieldParent;
				} else if (subrecord.signature == SIG_PNAM && payload.size() >= 2) {
					info.parentUseFlags = ReadLE<std::uint16_t>(payload, 0);
					info.fields |= WorldInfo::FieldParentUse;
				} else if (subrecord.signature == SIG_DATA && payload.size() >= 1) {
					info.dataFlags = payload[0];
					info.fields |= WorldInfo::FieldData;
				} else if (subrecord.signature == SIG_DNAM && payload.size() >= 8) {
					info.defaultLandHeight = ReadLE<float>(payload, 0);
					info.defaultWaterHeight = ReadLE<float>(payload, 4);
					info.fields |= WorldInfo::FieldHeights;
				} else if (subrecord.signature == SIG_NAM0 && payload.size() >= 8) {
					info.boundsMin = std::array<float, 2>{ ReadLE<float>(payload, 0), ReadLE<float>(payload, 4) };
					info.fields |= WorldInfo::FieldBoundsMin;
				} else if (subrecord.signature == SIG_NAM9 && payload.size() >= 8) {
					info.boundsMax = std::array<float, 2>{ ReadLE<float>(payload, 0), ReadLE<float>(payload, 4) };
					info.fields |= WorldInfo::FieldBoundsMax;
				} else if (subrecord.signature == SIG_NAM3 && payload.size() >= 4) {
					info.lodWaterType = toLoadOrder(payload);
					info.fields |= WorldInfo::FieldLodWaterType;
				} else if (subrecord.signature == SIG_NAM4 && payload.size() >= 4) {
					info.lodWaterHeight = ReadLE<float>(payload, 0);
					info.fields |= WorldInfo::FieldLodWaterHeight;
				} else if (subrecord.signature == SIG_NAMA && payload.size() >= 4) {
					info.distantLodMultiplier = ReadLE<float>(payload, 0);
					info.fields |= WorldInfo::FieldLodMultiplier;
				} else if (subrecord.signature == SIG_TNAM) {
					info.hdLodDiffuse = ReadString(payload);
					info.fields |= WorldInfo::FieldHdLodDiffuse;
				} else if (subrecord.signature == SIG_UNAM) {
					info.hdLodNormal = ReadString(payload);
					info.fields |= WorldInfo::FieldHdLodNormal;
				} else if (subrecord.signature == SIG_RNAM && payload.size() >= 8) {
					info.fields |= WorldInfo::FieldLargeReferences;
					// Large references, grouped by the cell that loads them: cell Y, cell X, a count, then each REFR
					// with the cell it is in.
					const auto count = ReadLE<std::uint32_t>(payload, 4);
					for (std::size_t i = 0, offset = 8; i < count && offset + 8 <= payload.size(); ++i, offset += 8) {
						info.largeReferences.push_back(LargeReference{
							.reference = toLoadOrder(payload.subspan(offset)),
							.cellY = ReadLE<std::int16_t>(payload, offset + 4),
							.cellX = ReadLE<std::int16_t>(payload, offset + 6),
						});
					}
				}
			}
			return info;
		}

		[[nodiscard]] BaseObjectInfo ExtractBaseObjectFromData(
			const RecordHeader& a_header,
			FormID a_formID,
			FileID a_sourceFileID,
			const std::shared_ptr<const std::vector<FileID>>& a_sourceMasterFileIDs,
			std::span<const std::uint8_t> a_data)
		{
			BaseObjectInfo info{};
			info.formID = a_formID;
			info.signature = a_header.signature;
			info.kind = BaseKindForSignature(a_header.signature);
			info.flags = a_header.flags;

			SubrecordCursor cursor(a_data);
			RawSubrecord subrecord{};
			std::span<const std::uint8_t> payload;
			while (cursor.Next(subrecord, payload)) {
				if (subrecord.signature == SIG_EDID) {
					info.editorID = ReadString(payload);
				} else if (subrecord.signature == SIG_OBND && payload.size() >= 12) {
					info.bounds = ReadObjectBounds(payload);
				} else if (subrecord.signature == SIG_MODL) {
					info.modelPath = ReadString(payload);
				} else if (a_header.signature == SIG_STAT && subrecord.signature == SIG_DNAM && payload.size() >= 8) {
					info.material = ConvertContextLocalFormID(FormID{ ReadLE<std::uint32_t>(payload, 4) }, a_sourceFileID, a_sourceMasterFileIDs);
				} else if (a_header.signature == SIG_STAT && subrecord.signature == SIG_MNAM) {
					for (std::size_t i = 0; i < info.lodModels.size(); ++i) {
						info.lodModels[i] = ReadString260(payload, i);
					}
				} else if (a_header.signature == SIG_SCOL && subrecord.signature == SIG_ONAM && payload.size() >= 4) {
					info.parts.push_back(CollectionPart{
						.baseFormID = ConvertContextLocalFormID(FormID{ ReadLE<std::uint32_t>(payload, 0) }, a_sourceFileID, a_sourceMasterFileIDs) });
				} else if (a_header.signature == SIG_SCOL && subrecord.signature == SIG_DATA && !info.parts.empty()) {
					ReadCollectionPlacements(info.parts.back(), payload);
				} else if (IsModelTextureSwapSubrecord(subrecord.signature)) {
					ParseAlternateTextures(
						info.alternateTextures,
						subrecord.signature,
						payload,
						nullptr,
						a_sourceFileID,
						a_sourceMasterFileIDs);
				}
			}
			return info;
		}

		[[nodiscard]] TextureSetInfo ExtractTextureSetFromData(
			const RecordHeader& a_header,
			FormID a_formID,
			std::span<const std::uint8_t> a_data)
		{
			TextureSetInfo info{};
			info.formID = a_formID;
			info.flags = a_header.flags;

			SubrecordCursor cursor(a_data);
			RawSubrecord subrecord{};
			std::span<const std::uint8_t> payload;
			while (cursor.Next(subrecord, payload)) {
				if (subrecord.signature == SIG_EDID) {
					info.editorID = ReadString(payload);
				} else if (const auto slot = TextureSlotIndexForSubrecord(subrecord.signature)) {
					info.texturePaths[*slot] = ReadString(payload);
				} else if (subrecord.signature == SIG_DNAM && payload.size() >= 2) {
					info.dnamFlags = ReadLE<std::uint16_t>(payload, 0);
				}
			}
			return info;
		}

		[[nodiscard]] CellInfo ExtractCellFromData(
			const RecordHeader& a_header,
			FormID a_formID,
			std::optional<FormID> a_parentWorld,
			std::span<const std::uint8_t> a_data)
		{
			CellInfo info{};
			info.formID = a_formID;
			info.worldFormID = a_parentWorld;
			info.flags = a_header.flags;

			SubrecordCursor cursor(a_data);
			RawSubrecord subrecord{};
			std::span<const std::uint8_t> payload;
			while (cursor.Next(subrecord, payload)) {
				if (subrecord.signature == SIG_EDID) {
					info.editorID = ReadString(payload);
					info.fields |= CellInfo::FieldEditorID;
				} else if (subrecord.signature == SIG_DATA) {
					if (payload.size() == 1) {
						info.cellFlags = payload[0];
						info.fields |= CellInfo::FieldData;
					} else if (payload.size() >= 2) {
						info.cellFlags = ReadLE<std::uint16_t>(payload, 0);
						info.fields |= CellInfo::FieldData;
					}
				} else if (subrecord.signature == SIG_XCLC && payload.size() >= 8) {
					info.gridX = ReadLE<std::int32_t>(payload, 0);
					info.gridY = ReadLE<std::int32_t>(payload, 4);
					info.forceHideLand = payload.size() >= 12 ? ReadLE<std::uint32_t>(payload, 8) : 0;
					info.fields |= CellInfo::FieldGrid;
				} else if (subrecord.signature == SIG_XCLW && payload.size() >= 4) {
					const auto waterHeight = ReadLE<float>(payload, 0);
					if (IsValidCellWaterHeight(waterHeight)) {
						info.waterHeight = waterHeight;
					}
					info.fields |= CellInfo::FieldWater;
				}
			}
			return info;
		}

		struct StreamingCellScan
		{
			CellInfo info;
			std::vector<std::uint8_t>* pending{ nullptr };
			std::size_t consumed{ 0 };
			std::size_t outputBytes{ 0 };
		};

		void ProcessCellSubrecord(StreamingCellScan& a_scan, FourCC a_signature, std::span<const std::uint8_t> a_payload)
		{
			if (a_signature == SIG_EDID) {
				a_scan.info.editorID = ReadString(a_payload);
				a_scan.info.fields |= CellInfo::FieldEditorID;
			} else if (a_signature == SIG_DATA) {
				if (a_payload.size() == 1) {
					a_scan.info.cellFlags = a_payload[0];
					a_scan.info.fields |= CellInfo::FieldData;
				} else if (a_payload.size() >= 2) {
					a_scan.info.cellFlags = ReadLE<std::uint16_t>(a_payload, 0);
					a_scan.info.fields |= CellInfo::FieldData;
				}
			} else if (a_signature == SIG_XCLC && a_payload.size() >= 8) {
				a_scan.info.gridX = ReadLE<std::int32_t>(a_payload, 0);
				a_scan.info.gridY = ReadLE<std::int32_t>(a_payload, 4);
				a_scan.info.forceHideLand = a_payload.size() >= 12 ? ReadLE<std::uint32_t>(a_payload, 8) : 0;
				a_scan.info.fields |= CellInfo::FieldGrid;
			} else if (a_signature == SIG_XCLW && a_payload.size() >= 4) {
				const auto waterHeight = ReadLE<float>(a_payload, 0);
				if (IsValidCellWaterHeight(waterHeight)) {
					a_scan.info.waterHeight = waterHeight;
				}
				a_scan.info.fields |= CellInfo::FieldWater;
			}
		}

		void AppendAndScanCellBytes(StreamingCellScan& a_scan, std::span<const std::uint8_t> a_bytes)
		{
			a_scan.outputBytes += a_bytes.size();
			auto& pending = *a_scan.pending;
			pending.insert(pending.end(), a_bytes.begin(), a_bytes.end());

			for (;;) {
				const auto available = pending.size() - a_scan.consumed;
				if (available < 6) {
					break;
				}

				const auto offset = a_scan.consumed;
				const auto pendingBytes = std::span<const std::uint8_t>(pending);
				auto signature = ReadLE<FourCC>(pendingBytes, offset);
				std::uint32_t size = ReadLE<std::uint16_t>(pendingBytes, offset + 4);
				std::size_t headerSize = 6;

				if (signature == SIG_XXXX) {
					if (available < 16) {
						break;
					}
					if (size != 4) throw std::runtime_error("invalid CELL XXXX subrecord");
					size = ReadLE<std::uint32_t>(pendingBytes, offset + 6);
					signature = ReadLE<FourCC>(pendingBytes, offset + 10);
					headerSize = 16;
				}

				const auto totalSize = headerSize + static_cast<std::size_t>(size);
				if (available < totalSize) {
					break;
				}

				ProcessCellSubrecord(
					a_scan,
					signature,
					std::span<const std::uint8_t>(pending).subspan(offset + headerSize, size));
				a_scan.consumed += totalSize;
			}

			if (a_scan.consumed > 4096 && a_scan.consumed * 2 >= pending.size()) {
				pending.erase(pending.begin(), pending.begin() + static_cast<std::ptrdiff_t>(a_scan.consumed));
				a_scan.consumed = 0;
			}
		}

		[[nodiscard]] CellInfo ExtractCellFromCompressedData(
			const RecordHeader& a_header,
			FormID a_formID,
			std::optional<FormID> a_parentWorld,
			std::span<const std::uint8_t> a_compressedData,
			std::vector<std::uint8_t>& a_pendingScratch,
			InflateStream& a_inflateStream,
			StaticPluginStats& a_stats)
		{
			if (a_compressedData.size() < sizeof(std::uint32_t)) {
				throw std::runtime_error("compressed CELL record is missing uncompressed size");
			}

			StreamingCellScan scan{};
			scan.info.formID = a_formID;
			scan.info.worldFormID = a_parentWorld;
			scan.info.flags = a_header.flags;
			a_pendingScratch.clear();
			if (a_pendingScratch.capacity() < 256) {
				a_pendingScratch.reserve(256);
			}
			scan.pending = std::addressof(a_pendingScratch);

			a_inflateStream.Reset(a_compressedData);
			auto& stream = a_inflateStream.stream;

			std::array<std::uint8_t, 64> output{};
			const auto expectedSize = ReadLE<std::uint32_t>(a_compressedData, 0);
			for (;;) {
				stream.next_out = reinterpret_cast<Bytef*>(output.data());
				stream.avail_out = static_cast<uInt>(output.size());
				const auto result = inflate(std::addressof(stream), Z_NO_FLUSH);
				if (result != Z_OK && result != Z_STREAM_END) {
					throw std::runtime_error("zlib failed to stream inflate CELL record");
				}

				const auto produced = output.size() - stream.avail_out;
				if (produced != 0) AppendAndScanCellBytes(scan, std::span<const std::uint8_t>(output.data(), produced));
				if (scan.outputBytes > expectedSize) throw std::runtime_error("compressed CELL exceeds advertised size");
				if (result == Z_STREAM_END) {
					break;
				}
				if (produced == 0 && stream.avail_in == 0) throw std::runtime_error("truncated compressed CELL stream");
			}

			++a_stats.streamedCellRecords;
			a_stats.streamedCellOutputBytes += scan.outputBytes;
			if (scan.outputBytes != expectedSize || scan.consumed != a_pendingScratch.size() || stream.avail_in != 0)
				throw std::runtime_error("invalid compressed CELL length or trailing data");
			return scan.info;
		}

		[[nodiscard]] std::optional<PlacementInfo> ExtractPlacementFromData(
			const RecordHeader& a_header,
			FormID a_formID,
			std::optional<FormID> a_parentWorld,
			std::optional<FormID> a_parentCell,
			FileID a_sourceFileID,
			const std::shared_ptr<const std::vector<FileID>>& a_sourceMasterFileIDs,
			std::span<const std::uint8_t> a_data)
		{
			PlacementInfo info{};
			info.formID = a_formID;
			info.parentCell = a_parentCell.value_or(FormID{});
			info.worldFormID = a_parentWorld;
			info.signature = a_header.signature;
			info.flags = a_header.flags;
			info.sourceFile = a_sourceFileID;
			info.formVersion = a_header.formVersion;

			bool hasBase = false;
			SubrecordCursor cursor(a_data);
			RawSubrecord subrecord{};
			std::span<const std::uint8_t> payload;
			while (cursor.Next(subrecord, payload)) {
				if (subrecord.signature == SIG_NAME && payload.size() >= 4) {
					info.baseFormID = ConvertContextLocalFormID(FormID{ ReadLE<std::uint32_t>(payload, 0) }, a_sourceFileID, a_sourceMasterFileIDs);
					hasBase = true;
				} else if (subrecord.signature == SIG_DATA && payload.size() >= 24) {
					for (std::size_t i = 0; i < 3; ++i) {
						info.position[i] = ReadLE<float>(payload, i * 4);
						info.rotation[i] = ReadLE<float>(payload, 12 + i * 4);
					}
				} else if (subrecord.signature == SIG_XSCL && payload.size() >= 4) {
					info.scale = ReadLE<float>(payload, 0);
				} else if (subrecord.signature == SIG_XESP && payload.size() >= 8) {
					info.enableParent = ConvertContextLocalFormID(FormID{ ReadLE<std::uint32_t>(payload, 0) }, a_sourceFileID, a_sourceMasterFileIDs);
					info.enableParentFlags = ReadLE<std::uint32_t>(payload, 4);
				} else if (subrecord.signature == SIG_XEMI && payload.size() >= 4) {
					info.emittance = ConvertContextLocalFormID(FormID{ReadLE<std::uint32_t>(payload, 0)}, a_sourceFileID, a_sourceMasterFileIDs);
				}
			}

			if (!hasBase) {
				return std::nullopt;
			}
			return info;
		}

		void AddSuppressor(
			StaticPluginShard& a_shard,
			const RecordHeader& a_header,
			FormID a_formID,
			std::size_t a_sourceFileIndex)
		{
			if (a_formID.IsNull() || a_formID.IsNone()) {
				return;
			}
			a_shard.suppressors.push_back(StaticRecordSuppressor{
				.formID = a_formID,
				.signature = a_header.signature,
				.flags = a_header.flags,
				.sourceFileIndex = a_sourceFileIndex,
			});
			++a_shard.stats.suppressors;
		}

		void ParseStaticRecord(std::span<const std::uint8_t> a_bytes, std::size_t a_offset, StaticPluginShard& a_shard, ParserContext& a_context)
		{
			const auto header = ReadRecordHeader(a_bytes, a_offset);
			++a_shard.stats.recordsScanned;

			const auto needsTypedPayload =
				header.signature == SIG_MATO || header.signature == SIG_REGN ||
				header.signature == SIG_MATT ||
				header.signature == SIG_WRLD ||
				header.signature == SIG_CELL ||
				header.signature == SIG_LAND ||
				header.signature == SIG_REFR ||
				header.signature == SIG_ACHR ||
				header.signature == SIG_TXST ||
				header.signature == SIG_LTEX ||
				header.signature == SIG_GRAS ||
				IsBaseObjectSignature(header.signature);
			if (!needsTypedPayload) {
				return;
			}

			const auto loadOrderFormID = ConvertContextLocalFormID(header.formID, a_context.entry.fileID, a_context.sourceMasterFileIDs);
			const auto suppressed = IsSuppressedFlags(header.flags);
			if (suppressed) {
				AddSuppressor(a_shard, header, loadOrderFormID, a_context.sourceFileIndex);
				if (header.signature == SIG_WRLD) {
					a_context.currentWorld = loadOrderFormID;
				} else if (header.signature == SIG_CELL) {
					a_context.currentCell = loadOrderFormID;
				}
				return;
			}

			std::span<const std::uint8_t> recordData = a_bytes.subspan(a_offset + 24, header.dataSize);
			if ((header.flags & RECORD_FLAG_COMPRESSED) != 0) {
				const auto uncompressedSize = recordData.size() >= sizeof(std::uint32_t) ? ReadLE<std::uint32_t>(recordData, 0) : 0u;
				const auto inflateBegin = Clock::now();
				++a_shard.stats.compressedRecords;
				if (IsBaseObjectSignature(header.signature)) {
					++a_shard.stats.compressedBaseRecords;
				} else if (header.signature == SIG_WRLD) {
					++a_shard.stats.compressedWorldRecords;
				} else if (header.signature == SIG_CELL) {
					++a_shard.stats.compressedCellRecords;
				} else if (header.signature == SIG_LAND) {
					++a_shard.stats.compressedLandRecords;
				} else if (header.signature == SIG_REFR || header.signature == SIG_ACHR) {
					++a_shard.stats.compressedPlacementRecords;
				}

				if (header.signature == SIG_CELL) {
					a_shard.cells.push_back(ExtractCellFromCompressedData(
						header,
						loadOrderFormID,
						a_context.currentWorld,
						recordData,
						a_context.cellScanScratch,
						a_context.cellInflateStream,
						a_shard.stats));
					a_shard.stats.inflateMilliseconds += ElapsedMilliseconds(inflateBegin);
					a_shard.stats.uncompressedRecordBytes += uncompressedSize;
					++a_shard.stats.typedRecords;
					++a_shard.stats.cells;
					a_context.currentCell = loadOrderFormID;
					return;
				}

				InflateRecordInto(recordData, a_context.inflateScratch);
				a_shard.stats.inflateMilliseconds += ElapsedMilliseconds(inflateBegin);
				recordData = a_context.inflateScratch;
			}
			a_shard.stats.uncompressedRecordBytes += recordData.size();
			++a_shard.stats.typedRecords;

			if (header.signature == SIG_MATO || header.signature == SIG_REGN) {
				EditorIDInfo info{ .formID = loadOrderFormID };
				SubrecordCursor cursor(recordData);
				RawSubrecord subrecord{};
				std::span<const std::uint8_t> payload;
				while (cursor.Next(subrecord, payload)) {
					if (subrecord.signature == SIG_EDID) {
						info.editorID = ReadString(payload);
						break;
					}
				}
				(header.signature == SIG_REGN ? a_shard.regions : a_shard.materialObjects).push_back(std::move(info));
			} else if (header.signature == SIG_MATT) {
				MaterialTypeInfo info{ .formID = loadOrderFormID };
				SubrecordCursor cursor(recordData);
				RawSubrecord subrecord{};
				std::span<const std::uint8_t> payload;
				while (cursor.Next(subrecord, payload)) {
					if (subrecord.signature == SIG_EDID) {
						info.editorID = ReadString(payload);
					} else if (subrecord.signature == SIG_MNAM) {
						info.name = ReadString(payload);
					}
				}
				a_shard.materialTypes.push_back(std::move(info));
			} else if (header.signature == SIG_WRLD) {
				a_shard.worlds.push_back(ExtractWorldFromData(
					header,
					loadOrderFormID,
					a_context.entry.fileID,
					a_context.sourceMasterFileIDs,
					recordData));
				++a_shard.stats.worlds;
				a_context.currentWorld = loadOrderFormID;
			} else if (header.signature == SIG_CELL) {
				a_shard.cells.push_back(ExtractCellFromData(header, loadOrderFormID, a_context.currentWorld, recordData));
				++a_shard.stats.cells;
				a_context.currentCell = loadOrderFormID;
			} else if (header.signature == SIG_LAND) {
				a_shard.lands.push_back(ExtractLandFromData(
					header,
					loadOrderFormID,
					a_context.currentWorld,
					a_context.currentCell,
					a_context.entry.fileID,
					a_context.sourceMasterFileIDs,
					recordData, a_context.mode == PluginParser::ParseMode::StaticGrassIndex));
				++a_shard.stats.lands;
			} else if (header.signature == SIG_REFR || header.signature == SIG_ACHR) {
				if (auto placement = ExtractPlacementFromData(
						header,
						loadOrderFormID,
						a_context.currentWorld,
						a_context.currentCell,
						a_context.entry.fileID,
						a_context.sourceMasterFileIDs,
						recordData)) {
					a_shard.placements.push_back(std::move(*placement));
					++a_shard.stats.placements;
				}
			} else if (IsBaseObjectSignature(header.signature)) {
				a_shard.baseObjects.push_back(ExtractBaseObjectFromData(
					header,
					loadOrderFormID,
					a_context.entry.fileID,
					a_context.sourceMasterFileIDs,
					recordData));
				++a_shard.stats.baseObjects;
			} else if (header.signature == SIG_TXST) {
				a_shard.textureSets.push_back(ExtractTextureSetFromData(header, loadOrderFormID, recordData));
			} else if (header.signature == SIG_LTEX) {
				a_shard.landTextures.push_back(ExtractLandTextureFromData(
					header,
					loadOrderFormID,
					a_context.entry.fileID,
					a_context.sourceMasterFileIDs,
					recordData));
				++a_shard.stats.landTextures;
			} else if (header.signature == SIG_GRAS) {
				a_shard.grasses.push_back(ExtractGrassFromData(header, loadOrderFormID, recordData));
			}
		}

		void ParseStaticRange(std::span<const std::uint8_t> a_bytes, std::size_t a_begin, std::size_t a_end, StaticPluginShard& a_shard, ParserContext& a_context)
		{
			std::size_t offset = a_begin;
			while (offset + 24 <= a_end) {
				const auto signature = ReadLE<FourCC>(a_bytes, offset);
				if (signature == SIG_GRUP) {
					const auto group = ReadGroupHeader(a_bytes, offset);
					if (group.groupSize < 24 || offset + group.groupSize > a_end) {
						throw std::runtime_error("invalid GRUP size in " + a_shard.entry.pluginName);
					}

					const auto savedWorld = a_context.currentWorld;
					const auto savedCell = a_context.currentCell;
					UpdateContextForGroup(a_context, group);
					ParseStaticRange(a_bytes, offset + 24, offset + group.groupSize, a_shard, a_context);
					a_context.currentWorld = savedWorld;
					a_context.currentCell = savedCell;
					offset += group.groupSize;
				} else {
					const auto header = ReadRecordHeader(a_bytes, offset);
					if (offset + 24 + header.dataSize > a_end) {
						throw std::runtime_error("invalid record size in " + a_shard.entry.pluginName);
					}
					ParseStaticRecord(a_bytes, offset, a_shard, a_context);
					offset += 24 + header.dataSize;
				}
			}
		}

		// A run of whole items (records and groups) of one range of a plugin, with the world and cell its first item is
		// parsed in, as the sequential parse would have them there: a part of a large plugin parsed by itself.
		struct StaticUnit
		{
			std::size_t begin{ 0 };
			std::size_t end{ 0 };
			std::optional<FormID> world;
			std::optional<FormID> cell;
		};

		// Groups this large are split into their items; a unit takes items until it's this large.
		constexpr std::size_t kUnitBytes = 1u << 20;

		// Splits a range into units, replaying what changes the world and cell as the parse would: a group's type and
		// label on the way in (restored on the way out), a WRLD or CELL record for the items after it.
		void SplitStaticRange(std::span<const std::uint8_t> a_bytes, std::size_t a_begin, std::size_t a_end, std::optional<FormID> a_world,
			std::optional<FormID> a_cell, const ParserContext& a_context, std::vector<StaticUnit>& a_units)
		{
			std::optional<StaticUnit> pending;
			const auto flush = [&](std::size_t a_at) {
				if (!pending) return;
				pending->end = a_at;
				a_units.push_back(*pending);
				pending.reset();
			};
			const auto take = [&](std::size_t a_at) {
				if (!pending) pending = StaticUnit{ .begin = a_at, .world = a_world, .cell = a_cell };
			};
			std::size_t offset = a_begin;
			while (offset + 24 <= a_end) {
				if (ReadLE<FourCC>(a_bytes, offset) == SIG_GRUP) {
					const auto group = ReadGroupHeader(a_bytes, offset);
					// A broken group is the parse's to report: the rest goes as one unit.
					if (group.groupSize < 24 || offset + group.groupSize > a_end) break;
					if (group.groupSize >= kUnitBytes) {
						flush(offset);
						auto world = a_world, cell = a_cell;
						if (group.type == 1 || group.type == 6 || group.type == 8 || group.type == 9 || group.type == 10) {
							const auto parent = ConvertContextLocalFormID(FormID{ group.label }, a_context.entry.fileID, a_context.sourceMasterFileIDs);
							(group.type == 1 ? world : cell) = parent;
						}
						SplitStaticRange(a_bytes, offset + 24, offset + group.groupSize, world, cell, a_context, a_units);
					} else {
						take(offset);
					}
					offset += group.groupSize;
				} else {
					const auto header = ReadRecordHeader(a_bytes, offset);
					if (offset + 24 + header.dataSize > a_end) break;
					take(offset);
					if (header.signature == SIG_WRLD || header.signature == SIG_CELL) {
						const auto formID = ConvertContextLocalFormID(header.formID, a_context.entry.fileID, a_context.sourceMasterFileIDs);
						(header.signature == SIG_WRLD ? a_world : a_cell) = formID;
					}
					offset += 24 + header.dataSize;
				}
				if (pending && offset - pending->begin >= kUnitBytes) flush(offset);
			}
			// The last unit takes the range's end, and whatever the parse makes of it.
			if (offset < a_end) take(offset);
			flush(a_end);
		}

		// Room in a_to for every part's items, so they're merged without growing.
		void Reserve(StaticPluginShard& a_to, std::span<const StaticPluginShard> a_parts)
		{
			const auto reserve = [&](auto a_member) {
				std::size_t size = (a_to.*a_member).size();
				for (const auto& part : a_parts) size += (part.*a_member).size();
				(a_to.*a_member).reserve(size);
			};
			reserve(&StaticPluginShard::baseObjects);
			reserve(&StaticPluginShard::textureSets);
			reserve(&StaticPluginShard::landTextures);
			reserve(&StaticPluginShard::grasses);
			reserve(&StaticPluginShard::worlds);
			reserve(&StaticPluginShard::materialObjects);
			reserve(&StaticPluginShard::regions);
			reserve(&StaticPluginShard::materialTypes);
			reserve(&StaticPluginShard::cells);
			reserve(&StaticPluginShard::lands);
			reserve(&StaticPluginShard::placements);
			reserve(&StaticPluginShard::suppressors);
		}

		void Append(StaticPluginShard& a_to, StaticPluginShard&& a_from)
		{
			const auto move = [](auto& a_into, auto& a_items) { a_into.insert(a_into.end(), std::make_move_iterator(a_items.begin()), std::make_move_iterator(a_items.end())); };
			move(a_to.baseObjects, a_from.baseObjects);
			move(a_to.textureSets, a_from.textureSets);
			move(a_to.landTextures, a_from.landTextures);
			move(a_to.grasses, a_from.grasses);
			move(a_to.worlds, a_from.worlds);
			move(a_to.materialObjects, a_from.materialObjects);
			move(a_to.regions, a_from.regions);
			move(a_to.materialTypes, a_from.materialTypes);
			move(a_to.cells, a_from.cells);
			move(a_to.lands, a_from.lands);
			move(a_to.placements, a_from.placements);
			move(a_to.suppressors, a_from.suppressors);
			auto& to = a_to.stats;
			const auto& from = a_from.stats;
			to.recordsScanned += from.recordsScanned;
			to.typedRecords += from.typedRecords;
			to.baseObjects += from.baseObjects;
			to.worlds += from.worlds;
			to.cells += from.cells;
			to.lands += from.lands;
			to.landTextures += from.landTextures;
			to.placements += from.placements;
			to.suppressors += from.suppressors;
			to.compressedRecords += from.compressedRecords;
			to.compressedBaseRecords += from.compressedBaseRecords;
			to.compressedWorldRecords += from.compressedWorldRecords;
			to.compressedCellRecords += from.compressedCellRecords;
			to.compressedLandRecords += from.compressedLandRecords;
			to.compressedPlacementRecords += from.compressedPlacementRecords;
			to.streamedCellRecords += from.streamedCellRecords;
			to.streamedCellEarlyStops += from.streamedCellEarlyStops;
			to.streamedCellOutputBytes += from.streamedCellOutputBytes;
			to.uncompressedRecordBytes += from.uncompressedRecordBytes;
			to.inflateMilliseconds += from.inflateMilliseconds;
		}
	}

	std::string FourCCString(FourCC a_value)
	{
		std::string result(4, '\0');
		result[0] = static_cast<char>(a_value & 0xFFu);
		result[1] = static_cast<char>((a_value >> 8) & 0xFFu);
		result[2] = static_cast<char>((a_value >> 16) & 0xFFu);
		result[3] = static_cast<char>((a_value >> 24) & 0xFFu);
		return result;
	}

	FourCC FourCCFromString(std::string_view a_value)
	{
		if (a_value.size() != 4) {
			throw std::runtime_error("FourCC string must be exactly four characters");
		}
		return MakeFourCC(a_value[0], a_value[1], a_value[2], a_value[3]);
	}

	std::string LowerAscii(std::string_view a_value)
	{
		std::string result(a_value);
		std::ranges::transform(result, result.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
		return result;
	}

	std::string Trim(std::string_view a_value)
	{
		auto begin = a_value.begin();
		auto end = a_value.end();
		while (begin != end && std::isspace(static_cast<unsigned char>(*begin))) {
			++begin;
		}
		while (begin != end && std::isspace(static_cast<unsigned char>(*(end - 1)))) {
			--end;
		}
		return std::string(begin, end);
	}

	std::uint32_t FileID::BaseFormID() const
	{
		if (kind == ModuleKind::Light) {
			return 0xFE000000u | (static_cast<std::uint32_t>(slot & 0x0FFFu) << 12);
		}
		return static_cast<std::uint32_t>(slot) << 24;
	}

	std::uint32_t FileID::ObjectMask() const
	{
		return kind == ModuleKind::Light ? 0xFFFu : 0xFFFFFFu;
	}

	std::string FileID::ToString() const
	{
		std::ostringstream out;
		out << std::hex << std::uppercase;
		if (kind == ModuleKind::Light) {
			out << "FE " << std::setw(3) << std::setfill('0') << slot;
		} else {
			out << std::setw(2) << std::setfill('0') << slot;
		}
		return out.str();
	}

	FileID FormID::GetFileID() const
	{
		const auto fullSlot = static_cast<std::uint8_t>(value >> 24);
		if (fullSlot == 0xFE) {
			return FileID{ .kind = ModuleKind::Light, .slot = static_cast<std::uint16_t>((value >> 12) & 0xFFFu) };
		}
		return FileID{ .kind = ModuleKind::Full, .slot = fullSlot };
	}

	std::uint32_t FormID::ObjectID() const
	{
		const auto fileID = GetFileID();
		return value & fileID.ObjectMask();
	}

	FormID FormID::WithFileID(FileID a_fileID) const
	{
		return FormID{ a_fileID.BaseFormID() | (ObjectID() & a_fileID.ObjectMask()) };
	}

	std::string FormID::ToString() const
	{
		std::ostringstream out;
		out << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << value;
		return out.str();
	}

	ExplicitLoadOrderSource::ExplicitLoadOrderSource(std::vector<std::filesystem::path> a_pluginPaths) :
		_pluginPaths(std::move(a_pluginPaths))
	{}

	std::vector<LoadOrderEntry> ExplicitLoadOrderSource::Load()
	{
		std::vector<LoadOrderEntry> result;
		result.reserve(_pluginPaths.size());
		for (const auto& path : _pluginPaths) {
			if (!IsPluginExtension(platform::PathToUtf8(path.filename()))) {
				continue;
			}
			result.push_back(LoadOrderEntry{
				.path = path,
				.pluginName = PluginNameFromPath(path),
			});
		}
		return result;
	}

	PluginsTxtLoadOrderSource::PluginsTxtLoadOrderSource(std::filesystem::path a_dataPath, std::filesystem::path a_pluginsTxtPath) :
		_dataPath(std::move(a_dataPath)),
		_pluginsTxtPath(std::move(a_pluginsTxtPath))
	{}

	std::vector<LoadOrderEntry> PluginsTxtLoadOrderSource::Load()
	{
		std::ifstream input(_pluginsTxtPath);
		if (!input) {
			throw std::runtime_error("failed to open plugins.txt: " + platform::PathToUtf8(_pluginsTxtPath));
		}

		std::vector<LoadOrderEntry> result;
		std::unordered_map<std::string, std::uint8_t> added;
		const auto appendIfPresent = [&](std::string_view a_pluginName) {
			const auto key = LowerAscii(a_pluginName);
			if (!IsPluginExtension(key) || added.contains(key)) {
				return;
			}
			const auto path = _dataPath / key;
			if (!std::filesystem::exists(path)) {
				return;
			}
			result.push_back(LoadOrderEntry{
				.path = path,
				.pluginName = key,
			});
			added.emplace(key, 0);
		};

		appendIfPresent("Skyrim.esm");
		appendIfPresent("Update.esm");
		appendIfPresent("Dawnguard.esm");
		appendIfPresent("HearthFires.esm");
		appendIfPresent("Dragonborn.esm");

		const auto cccPath = _dataPath.parent_path() / "Skyrim.ccc";
		if (std::filesystem::exists(cccPath)) {
			std::ifstream cccInput(cccPath);
			std::string cccLine;
			while (std::getline(cccInput, cccLine)) {
				const auto trimmed = Trim(cccLine);
				if (!trimmed.empty() && !trimmed.starts_with('#')) {
					appendIfPresent(trimmed);
				}
			}
		}

		std::string line;
		while (std::getline(input, line)) {
			line = Trim(line);
			if (line.empty() || line.starts_with('#')) {
				continue;
			}
			// Skyrim SE/AE and MO2 prefix enabled entries with '*'. Unstarred
			// entries remain in plugins.txt to preserve their ordering, but are
			// disabled and must not contribute records to the resolved load order.
			if (!line.starts_with('*')) {
				continue;
			}
			line = Trim(std::string_view(line).substr(1));
			if (!IsPluginExtension(line)) {
				continue;
			}
			appendIfPresent(line);
		}
		return result;
	}

	const RawRecord* OverrideChain::Winning() const
	{
		return overrides.empty() ? firstRecord : overrides.back();
	}

	const RawRecord* OverrideChain::VisibleFrom(std::size_t a_sourceFileIndex) const
	{
		const RawRecord* result = nullptr;
		if (firstRecord && firstRecord->sourceFileIndex <= a_sourceFileIndex) {
			result = firstRecord;
		}
		for (const auto* record : overrides) {
			if (record && record->sourceFileIndex <= a_sourceFileIndex) {
				result = record;
			}
		}
		return result;
	}

	const OverrideChain* ResolvedRecordStore::Find(FormID a_id) const
	{
		const auto it = chains.find(a_id);
		return it == chains.end() ? nullptr : std::addressof(it->second);
	}

	const RawRecord* ResolvedRecordStore::Winning(FormID a_id) const
	{
		const auto* chain = Find(a_id);
		return chain ? chain->Winning() : nullptr;
	}

	const RawRecord* ResolvedRecordStore::VisibleFrom(FormID a_id, std::size_t a_sourceFileIndex) const
	{
		const auto* chain = Find(a_id);
		return chain ? chain->VisibleFrom(a_sourceFileIndex) : nullptr;
	}

	std::size_t CellKeyHash::operator()(const CellKey& a_key) const noexcept
	{
		auto value = static_cast<std::size_t>(a_key.worldFormID.value);
		value ^= static_cast<std::size_t>(static_cast<std::uint32_t>(a_key.x)) + 0x9e3779b9u + (value << 6) + (value >> 2);
		value ^= static_cast<std::size_t>(static_cast<std::uint32_t>(a_key.y)) + 0x9e3779b9u + (value << 6) + (value >> 2);
		return value;
	}

	std::size_t GameDataSnapshot::RawRecordCount() const
	{
		std::size_t result = 0;
		for (const auto& plugin : plugins) {
			result += plugin.records.size();
		}
		return result;
	}

	std::optional<PluginHeader> HeaderCache::Find(
		const std::filesystem::path& a_path,
		std::filesystem::file_time_type a_writeTime,
		std::uintmax_t a_fileSize) const
	{
		std::scoped_lock lock(_mutex);
		const auto it = _entries.find(a_path.native());
		if (it != _entries.end() && it->second.writeTime == a_writeTime && it->second.fileSize == a_fileSize) {
			return it->second.header;
		}
		return std::nullopt;
	}

	void HeaderCache::Store(
		const std::filesystem::path& a_path,
		std::filesystem::file_time_type a_writeTime,
		std::uintmax_t a_fileSize,
		PluginHeader a_header)
	{
		std::scoped_lock lock(_mutex);
		_entries.insert_or_assign(a_path.native(), Entry{ a_writeTime, a_fileSize, std::move(a_header) });
	}

	PluginHeader PluginParser::ReadHeader(const std::filesystem::path& a_path) const
	{
		std::error_code ec;
		const auto writeTime = std::filesystem::last_write_time(a_path, ec);
		const auto fileSize = ec ? 0 : std::filesystem::file_size(a_path, ec);
		if (!ec && _headerCache) {
			if (auto cached = _headerCache->Find(a_path, writeTime, fileSize)) {
				return std::move(*cached);
			}
		}

		std::ifstream input(a_path, std::ios::binary);
		if (!input) {
			throw std::runtime_error("failed to open plugin file: " + platform::PathToUtf8(a_path));
		}

		std::array<std::uint8_t, 24> headerBytes{};
		input.read(reinterpret_cast<char*>(headerBytes.data()), static_cast<std::streamsize>(headerBytes.size()));
		if (input.gcount() != static_cast<std::streamsize>(headerBytes.size())) {
			throw std::runtime_error("plugin is too small: " + platform::PathToUtf8(a_path));
		}

		const auto header = ReadRecordHeader(headerBytes, 0);
		if (header.signature != SIG_TES4) {
			throw std::runtime_error("plugin does not start with TES4 header: " + platform::PathToUtf8(a_path));
		}
		std::vector<std::uint8_t> headerData(header.dataSize);
		if (!headerData.empty()) {
			input.read(reinterpret_cast<char*>(headerData.data()), static_cast<std::streamsize>(headerData.size()));
			if (input.gcount() != static_cast<std::streamsize>(headerData.size())) {
				throw std::runtime_error("TES4 header data exceeds file size: " + platform::PathToUtf8(a_path));
			}
		}

		PluginHeader result{};
		result.flags = header.flags;
		ParseHeaderRecordData(headerData, result);
		if (!ec && _headerCache) {
			_headerCache->Store(a_path, writeTime, fileSize, result);
		}
		return result;
	}

	namespace
	{
		// Rethrows a plugin read error with the plugin's file name in front, so a player can tell which plugin it is.
		// A message that already names the plugin is left as it is.
		[[noreturn]] void ThrowNamingPlugin(const LoadOrderEntry& a_entry, const std::exception& a_error)
		{
			const auto name = platform::PathToUtf8(a_entry.path.filename());
			const std::string_view what = a_error.what();
			if (what.find(name) != std::string_view::npos) {
				throw std::runtime_error(std::string(what));
			}
			throw std::runtime_error(name + ": " + std::string(what));
		}
	}

	PluginFile PluginParser::Parse(
		const LoadOrderEntry& a_entry,
		std::size_t a_sourceFileIndex,
		std::span<const LoadOrderEntry> a_loadOrder,
		ParseMode a_mode) const
	try {
		PluginFile plugin{};
		plugin.entry = a_entry;
		plugin.bytes = ReadFileBytes(a_entry.path);
		const auto bytes = std::span<const std::uint8_t>(plugin.bytes);
		if (bytes.size() < 24) {
			throw std::runtime_error("plugin is too small: " + platform::PathToUtf8(a_entry.path));
		}

		const auto tes4 = ReadRecordHeader(bytes, 0);
		if (tes4.signature != SIG_TES4) {
			throw std::runtime_error("plugin does not start with TES4 header: " + platform::PathToUtf8(a_entry.path));
		}
		plugin.header.flags = tes4.flags;
		ParseHeaderRecordData(std::span<const std::uint8_t>(bytes).subspan(24, tes4.dataSize), plugin.header);
		plugin.records.reserve(static_cast<std::size_t>(plugin.header.recordCount) + 1);

		ParserContext context{
			.entry = plugin.entry,
			.loadOrder = a_loadOrder,
			.mode = a_mode,
			.sourceFileIndex = a_sourceFileIndex,
			.sourceMasterFileIDs = BuildSourceMasterFileIDs(plugin.entry, a_loadOrder),
		};
		if (a_mode == ParseMode::FullRaw) {
			ParseRecord(bytes, 0, plugin, context);
		}
		ParseRange(bytes, 24 + tes4.dataSize, bytes.size(), plugin, context);
		return plugin;
	} catch (const std::exception& error) {
		ThrowNamingPlugin(a_entry, error);
	}

	StaticPluginShard PluginParser::ParseStaticWorldShard(
		const LoadOrderEntry& a_entry,
		std::size_t a_sourceFileIndex,
		std::span<const LoadOrderEntry> a_loadOrder, ParseMode a_mode) const
	try {
		const auto totalBegin = Clock::now();
		StaticPluginShard shard{};
		shard.entry = a_entry;
		const auto readBegin = Clock::now();
		auto fileBytes = MapFileBytes(a_entry.path);
		shard.stats.readMilliseconds = ElapsedMilliseconds(readBegin);
		const auto bytes = fileBytes.Span();
		shard.stats.fileBytes = bytes.size();
		if (bytes.size() < 24) {
			throw std::runtime_error("plugin is too small: " + platform::PathToUtf8(a_entry.path));
		}

		const auto tes4 = ReadRecordHeader(bytes, 0);
		if (tes4.signature != SIG_TES4) {
			throw std::runtime_error("plugin does not start with TES4 header: " + platform::PathToUtf8(a_entry.path));
		}
		shard.header.flags = a_entry.headerFlags != 0 ? a_entry.headerFlags : tes4.flags;
		shard.header.recordCount = a_entry.recordCount;
		if (shard.header.recordCount == 0) {
			ParseHeaderRecordData(bytes.subspan(24, tes4.dataSize), shard.header);
		}

		const auto reserveCount = static_cast<std::size_t>(shard.header.recordCount);
		shard.baseObjects.reserve(reserveCount / 12);
		shard.cells.reserve(reserveCount / 8);
		shard.placements.reserve(reserveCount / 2);
		shard.suppressors.reserve(reserveCount / 32);

		ParserContext context{
			.entry = shard.entry,
			.loadOrder = a_loadOrder,
			.mode = a_mode,
			.sourceFileIndex = a_sourceFileIndex,
			.sourceMasterFileIDs = BuildSourceMasterFileIDs(shard.entry, a_loadOrder),
		};
		const auto scanBegin = Clock::now();
		const auto begin = 24 + static_cast<std::size_t>(tes4.dataSize);
		std::vector<StaticUnit> units;
		// A large plugin (the game's masters) in parts on every core, merged in the file's order: the same shard.
		if (bytes.size() >= 8 * kUnitBytes) SplitStaticRange(bytes, begin, bytes.size(), std::nullopt, std::nullopt, context, units);
		if (units.size() > 1) {
			TW_ZONE("plugin parse parts");
			std::vector<StaticPluginShard> parts(units.size());
			oneapi::tbb::parallel_for(std::size_t{ 0 }, units.size(), std::size_t{ 1 }, [&](std::size_t i) {
				TW_ZONE("plugin parse part");
				parts[i].entry = shard.entry;
				ParserContext partContext{
					.entry = parts[i].entry,
					.loadOrder = a_loadOrder,
					.mode = a_mode,
					.sourceFileIndex = a_sourceFileIndex,
					.sourceMasterFileIDs = context.sourceMasterFileIDs,
					.currentWorld = units[i].world,
					.currentCell = units[i].cell,
				};
				ParseStaticRange(bytes, units[i].begin, units[i].end, parts[i], partContext);
			});
			Reserve(shard, parts);
			for (auto& part : parts) Append(shard, std::move(part));
		} else {
			ParseStaticRange(bytes, begin, bytes.size(), shard, context);
		}
		shard.stats.scanMilliseconds = ElapsedMilliseconds(scanBegin);
		shard.stats.totalMilliseconds = ElapsedMilliseconds(totalBegin);
		return shard;
	} catch (const std::exception& error) {
		ThrowNamingPlugin(a_entry, error);
	}

	std::vector<LoadOrderEntry> PrepareLoadOrder(std::vector<LoadOrderEntry> a_entries, HeaderCache* a_headerCache)
	{
		oneapi::tbb::parallel_for(
			oneapi::tbb::blocked_range<std::size_t>(0, a_entries.size()),
			[&](const oneapi::tbb::blocked_range<std::size_t>& range) {
				PluginParser parser{ a_headerCache };
				for (std::size_t i = range.begin(); i != range.end(); ++i) {
					auto& entry = a_entries[i];
					entry.pluginName = entry.pluginName.empty() ? PluginNameFromPath(entry.path) : LowerAscii(entry.pluginName);
					entry.masters.clear();
					const auto header = parser.ReadHeader(entry.path);
					entry.headerFlags = header.flags;
					entry.recordCount = header.recordCount;
					entry.kind = DetectModuleKind(entry.path, header);
					entry.masters.reserve(header.masters.size());
					for (auto master : header.masters) {
						entry.masters.push_back(LowerAscii(master));
					}
				}
			});

		std::uint16_t fullSlot = 0;
		std::uint16_t lightSlot = 0;
		for (auto& entry : a_entries) {
			if (entry.kind == ModuleKind::Light) {
				if (lightSlot > 0x0FFFu) {
					throw std::runtime_error("too many ESL/light plugins");
				}
				entry.fileID = FileID{ .kind = ModuleKind::Light, .slot = lightSlot++ };
			} else {
				if (fullSlot > 0xFDu) {
					const auto full = std::ranges::count_if(a_entries, [](const LoadOrderEntry& e) { return e.kind != ModuleKind::Light; });
                    throw std::runtime_error("too many full plugins: " + std::to_string(full) + " enabled; " + entry.path.filename().string() + " is the first past the limit of 254");
				}
				entry.fileID = FileID{ .kind = ModuleKind::Full, .slot = fullSlot++ };
			}
		}
		return a_entries;
	}

	ResolvedRecordStore BuildResolvedRecordStore(const std::vector<PluginFile>& a_plugins)
	{
		ResolvedRecordStore store;
		std::size_t recordCount = 0;
		for (const auto& plugin : a_plugins) {
			recordCount += plugin.records.size();
		}
		store.chains.reserve(recordCount);
		for (const auto& plugin : a_plugins) {
			for (const auto& record : plugin.records) {
				if (record.loadOrderFormID.IsNull() || record.loadOrderFormID.IsNone()) {
					continue;
				}
				auto [it, inserted] = store.chains.try_emplace(record.loadOrderFormID);
				auto& chain = it->second;
				if (inserted) {
					chain.formID = record.loadOrderFormID;
					chain.firstRecord = std::addressof(record);
				} else {
					chain.overrides.push_back(std::addressof(record));
				}
			}
		}
		return store;
	}

	namespace
	{
		struct LocalIndexRecords
		{
			std::vector<BaseObjectInfo> bases;
			std::vector<TextureSetInfo> textureSets;
			std::vector<LandTextureInfo> landTextures;
			std::vector<GrassInfo> grasses;
			std::vector<CellInfo> cells;
			std::vector<LandInfo> lands;
			std::vector<const RawRecord*> placementRecords;
		};

		struct LocalPlacementBuckets
		{
			std::vector<std::pair<CellKey, PlacementInfo>> exterior;
			std::vector<std::pair<FormID, PlacementInfo>> interior;
			std::vector<PlacementInfo> orphan;
		};

		struct LocalStaticPlacementBuckets
		{
			std::vector<std::pair<CellKey, PlacementInfo>> exterior;
			std::vector<std::pair<FormID, PlacementInfo>> interior;
			std::vector<PlacementInfo> orphan;
			std::size_t winning{ 0 };
			std::size_t disabled{ 0 };
			std::size_t missingBase{ 0 };
			std::size_t missingCell{ 0 };
		};

		[[nodiscard]] std::int32_t WorldCoordinateToCell(float a_coordinate)
		{
			return static_cast<std::int32_t>(std::floor(a_coordinate / kSkyrimTerrainCellSize));
		}

		[[nodiscard]] std::optional<CellKey> ResolvePlacementExteriorCellKey(
			const CellInfo& a_cell,
			const PlacementInfo& a_placement)
		{
			if (!a_cell.worldFormID || a_cell.IsInterior()) {
				return std::nullopt;
			}
			if (a_cell.gridX && a_cell.gridY) {
				return CellKey{ *a_cell.worldFormID, *a_cell.gridX, *a_cell.gridY };
			}
			return CellKey{
				*a_cell.worldFormID,
				WorldCoordinateToCell(a_placement.position[0]),
				WorldCoordinateToCell(a_placement.position[1])
			};
		}

		class FormIDSeenSet
		{
		public:
			explicit FormIDSeenSet(std::size_t a_expectedCount)
			{
				std::size_t capacity = 1;
				const auto target = (std::max<std::size_t>)(16, a_expectedCount * 2);
				while (capacity < target) {
					capacity <<= 1;
				}
				_slots.assign(capacity, 0);
				_mask = capacity - 1;
			}

			[[nodiscard]] bool Insert(FormID a_formID)
			{
				auto value = a_formID.value;
				if (value == 0) {
					return false;
				}

				auto index = Hash(value) & _mask;
				for (;;) {
					auto& slot = _slots[index];
					if (slot == value) {
						return false;
					}
					if (slot == 0) {
						slot = value;
						return true;
					}
					index = (index + 1) & _mask;
				}
			}

		private:
			[[nodiscard]] static std::size_t Hash(std::uint32_t a_value) noexcept
			{
				auto value = a_value;
				value ^= value >> 16;
				value *= 0x7feb352du;
				value ^= value >> 15;
				value *= 0x846ca68bu;
				value ^= value >> 16;
				return value;
			}

			std::vector<std::uint32_t> _slots;
			std::size_t _mask{ 0 };
		};

		[[nodiscard]] WorldIndex BuildWorldIndexFromWinningRecords(std::span<const RawRecord* const> a_winningRecords)
		{
			WorldIndex index;
			index.baseObjects.reserve(24000);
			index.textureSets.reserve(1024);
			index.landTextures.reserve(2048);
			index.grasses.reserve(2048);
			index.cells.reserve(90000);

			oneapi::tbb::enumerable_thread_specific<LocalIndexRecords> localRecords;
			oneapi::tbb::parallel_for(
				oneapi::tbb::blocked_range<std::size_t>(0, a_winningRecords.size(), 4096),
				[&](const oneapi::tbb::blocked_range<std::size_t>& range) {
					auto& local = localRecords.local();
					for (std::size_t i = range.begin(); i != range.end(); ++i) {
						const auto* record = a_winningRecords[i];
						if (!record) {
							continue;
						}
						if (record->signature == SIG_REFR || record->signature == SIG_ACHR) {
							local.placementRecords.push_back(record);
						} else if (record->signature == SIG_CELL) {
							if (auto cell = ExtractCell(*record)) {
								local.cells.push_back(std::move(*cell));
							}
						} else if (record->signature == SIG_LAND) {
							if (auto land = ExtractLand(*record)) {
								local.lands.push_back(std::move(*land));
							}
						} else if (IsBaseObjectSignature(record->signature)) {
							if (auto base = ExtractBaseObject(*record)) {
								local.bases.push_back(std::move(*base));
							}
						} else if (record->signature == SIG_TXST) {
							if (auto textureSet = ExtractTextureSet(*record)) {
								local.textureSets.push_back(std::move(*textureSet));
							}
						} else if (record->signature == SIG_LTEX) {
							if (auto landTexture = ExtractLandTexture(*record)) {
								local.landTextures.push_back(std::move(*landTexture));
							}
						} else if (record->signature == SIG_GRAS) {
							if (auto grass = ExtractGrass(*record)) {
								local.grasses.push_back(std::move(*grass));
							}
						}
					}
				});

			std::vector<const RawRecord*> placementRecords;
			std::vector<LandInfo> lands;
			for (auto& local : localRecords) {
				for (auto& base : local.bases) {
					index.baseObjects[base.formID] = std::move(base);
				}
				for (auto& textureSet : local.textureSets) {
					index.textureSets[textureSet.formID] = std::move(textureSet);
				}
				for (auto& landTexture : local.landTextures) {
					index.landTextures[landTexture.formID] = std::move(landTexture);
				}
				for (auto& grass : local.grasses) {
					index.grasses[grass.formID] = std::move(grass);
				}
				for (auto& cell : local.cells) {
					index.cells[cell.formID] = std::move(cell);
				}
				lands.insert(lands.end(), std::make_move_iterator(local.lands.begin()), std::make_move_iterator(local.lands.end()));
				placementRecords.insert(placementRecords.end(), local.placementRecords.begin(), local.placementRecords.end());
			}

			for (auto& land : lands) {
				const auto cellIt = index.cells.find(land.parentCell);
				if (cellIt == index.cells.end() || !cellIt->second.worldFormID || !cellIt->second.gridX || !cellIt->second.gridY) {
					continue;
				}
				land.worldFormID = cellIt->second.worldFormID;
				land.cellX = cellIt->second.gridX;
				land.cellY = cellIt->second.gridY;
				index.landsByWorldspace[*land.worldFormID].push_back(std::move(land));
			}

			oneapi::tbb::enumerable_thread_specific<LocalPlacementBuckets> localBuckets;
			oneapi::tbb::parallel_for(
				oneapi::tbb::blocked_range<std::size_t>(0, placementRecords.size(), 4096),
				[&](const oneapi::tbb::blocked_range<std::size_t>& range) {
					auto& local = localBuckets.local();
					for (std::size_t i = range.begin(); i != range.end(); ++i) {
						const auto* record = placementRecords[i];
						auto placement = ExtractPlacement(*record);
						if (!placement || placement->IsInitiallyDisabled()) {
							continue;
						}
						if (!index.baseObjects.contains(placement->baseFormID)) {
							continue;
						}
						const auto cellIt = index.cells.find(placement->parentCell);
						if (cellIt == index.cells.end()) {
							local.orphan.push_back(std::move(*placement));
							continue;
						}
						const auto& cell = cellIt->second;
						placement->worldFormID = cell.worldFormID;
						if (auto exteriorKey = ResolvePlacementExteriorCellKey(cell, *placement)) {
							local.exterior.emplace_back(*exteriorKey, std::move(*placement));
						} else {
							local.interior.emplace_back(cell.formID, std::move(*placement));
						}
					}
				});

			for (auto& local : localBuckets) {
				for (auto& [cellKey, placement] : local.exterior) {
					index.exteriorPlacements[cellKey].push_back(std::move(placement));
				}
				for (auto& [cellFormID, placement] : local.interior) {
					index.interiorPlacements[cellFormID].push_back(std::move(placement));
				}
				index.orphanPlacements.insert(index.orphanPlacements.end(), std::make_move_iterator(local.orphan.begin()), std::make_move_iterator(local.orphan.end()));
			}
			return index;
		}
	}

	WorldIndex BuildWorldIndex(const ResolvedRecordStore& a_resolvedRecords)
	{
		std::vector<const RawRecord*> winningRecords;
		winningRecords.reserve(a_resolvedRecords.chains.size());
		for (const auto& [_, chain] : a_resolvedRecords.chains) {
			winningRecords.push_back(chain.Winning());
		}
		return BuildWorldIndexFromWinningRecords(winningRecords);
	}

	WorldIndex BuildStaticWorldIndex(const std::vector<PluginFile>& a_plugins)
	{
		std::size_t recordCount = 0;
		for (const auto& plugin : a_plugins) {
			recordCount += plugin.records.size();
		}

		std::unordered_map<FormID, const RawRecord*, FormIDHash> winning;
		winning.reserve(recordCount);
		for (const auto& plugin : a_plugins) {
			for (const auto& record : plugin.records) {
				if (record.loadOrderFormID.IsNull() || record.loadOrderFormID.IsNone()) {
					continue;
				}
				winning.insert_or_assign(record.loadOrderFormID, std::addressof(record));
			}
		}

		std::vector<const RawRecord*> winningRecords;
		winningRecords.reserve(winning.size());
		for (const auto& [_, record] : winning) {
			winningRecords.push_back(record);
		}
		return BuildWorldIndexFromWinningRecords(winningRecords);
	}

	std::array<float, LandInfo::VertexCount> DecodeLandHeights(std::span<const std::uint8_t> a_vhgtPayload)
	{
		return DecodeLandHeightsInternal(a_vhgtPayload);
	}

	namespace
	{
		struct TerrainLatticeKey
		{
			std::int32_t x{ 0 };
			std::int32_t y{ 0 };

			friend bool operator==(const TerrainLatticeKey&, const TerrainLatticeKey&) = default;
		};

		struct TerrainLatticeKeyHash
		{
			std::size_t operator()(const TerrainLatticeKey& a_key) const noexcept
			{
				const auto x = static_cast<std::uint32_t>(a_key.x);
				const auto y = static_cast<std::uint32_t>(a_key.y);
				return (static_cast<std::size_t>(x) * 0x9E3779B1u) ^ (static_cast<std::size_t>(y) * 0x85EBCA77u);
			}
		};

		[[nodiscard]] float Clamp01(float a_value)
		{
			return std::clamp(a_value, 0.0f, 1.0f);
		}

		void AddNormal(TerrainVertex& a_vertex, float a_x, float a_y, float a_z)
		{
			a_vertex.normal[0] += a_x;
			a_vertex.normal[1] += a_y;
			a_vertex.normal[2] += a_z;
		}

		void AccumulateTriangleNormal(std::vector<TerrainVertex>& a_vertices, std::uint32_t a_i0, std::uint32_t a_i1, std::uint32_t a_i2)
		{
			auto& v0 = a_vertices[a_i0];
			auto& v1 = a_vertices[a_i1];
			auto& v2 = a_vertices[a_i2];
			const float ax = v1.position[0] - v0.position[0];
			const float ay = v1.position[1] - v0.position[1];
			const float az = v1.position[2] - v0.position[2];
			const float bx = v2.position[0] - v0.position[0];
			const float by = v2.position[1] - v0.position[1];
			const float bz = v2.position[2] - v0.position[2];
			const float nx = ay * bz - az * by;
			const float ny = az * bx - ax * bz;
			const float nz = ax * by - ay * bx;
			AddNormal(v0, nx, ny, nz);
			AddNormal(v1, nx, ny, nz);
			AddNormal(v2, nx, ny, nz);
		}

		[[nodiscard]] std::uint8_t PackWeight(float a_weight)
		{
			return static_cast<std::uint8_t>(std::lround(Clamp01(a_weight) * 255.0f));
		}

		void PackWeightByte(std::vector<std::uint32_t>& a_words, std::size_t a_baseWord, std::size_t a_sampleIndex, float a_weight)
		{
			const auto wordIndex = a_baseWord + a_sampleIndex / 4u;
			const auto byteShift = static_cast<std::uint32_t>((a_sampleIndex & 3u) * 8u);
			const auto byteValue = static_cast<std::uint32_t>(PackWeight(a_weight));
			a_words[wordIndex] &= ~(0xFFu << byteShift);
			a_words[wordIndex] |= byteValue << byteShift;
		}

		struct FinalizedTerrainQuadrantLayers
		{
			std::array<FormID, 7> layerFormIDs{};
			std::array<std::array<float, LandInfo::QuadrantVertexCount>, 7> weights{};
		};

		struct TerrainPaintSampleKey
		{
			std::int32_t x{ 0 };
			std::int32_t y{ 0 };
			FormID layerFormID{};

			friend bool operator==(const TerrainPaintSampleKey&, const TerrainPaintSampleKey&) = default;
		};

		struct TerrainPaintSampleKeyHash
		{
			std::size_t operator()(const TerrainPaintSampleKey& a_key) const noexcept
			{
				std::size_t hash = 1469598103934665603ull;
				const auto mix = [&](std::uint32_t value) {
					hash ^= value;
					hash *= 1099511628211ull;
				};
				mix(static_cast<std::uint32_t>(a_key.x));
				mix(static_cast<std::uint32_t>(a_key.y));
				mix(a_key.layerFormID.value);
				return hash;
			}
		};

		struct TerrainPaintLatticeKey
		{
			std::int32_t x{ 0 };
			std::int32_t y{ 0 };

			friend bool operator==(const TerrainPaintLatticeKey&, const TerrainPaintLatticeKey&) = default;
		};

		struct TerrainPaintLatticeKeyHash
		{
			std::size_t operator()(const TerrainPaintLatticeKey& a_key) const noexcept
			{
				std::size_t hash = 1469598103934665603ull;
				const auto mix = [&](std::uint32_t value) {
					hash ^= value;
					hash *= 1099511628211ull;
				};
				mix(static_cast<std::uint32_t>(a_key.x));
				mix(static_cast<std::uint32_t>(a_key.y));
				return hash;
			}
		};

		struct TerrainQuadrantPaintKey
		{
			std::int32_t cellX{ 0 };
			std::int32_t cellY{ 0 };
			std::uint8_t quadrant{ 0 };

			friend bool operator==(const TerrainQuadrantPaintKey&, const TerrainQuadrantPaintKey&) = default;
		};

		struct TerrainQuadrantPaintKeyHash
		{
			std::size_t operator()(const TerrainQuadrantPaintKey& a_key) const noexcept
			{
				std::size_t hash = 1469598103934665603ull;
				const auto mix = [&](std::uint32_t value) {
					hash ^= value;
					hash *= 1099511628211ull;
				};
				mix(static_cast<std::uint32_t>(a_key.cellX));
				mix(static_cast<std::uint32_t>(a_key.cellY));
				mix(a_key.quadrant);
				return hash;
			}
		};

		[[nodiscard]] float NormalizeSkyrimLandOpacity(float a_opacity)
		{
			// The runtime treats values above 1 as Creation Kit percentage-style weights.
			return Clamp01(a_opacity > 1.0f ? a_opacity * 0.01f : a_opacity);
		}

		template <std::size_t N>
		void RemoveRuntimeAlphaSlot(
			std::array<FormID, N>& a_alphaFormIDs,
			std::array<std::array<float, LandInfo::QuadrantVertexCount>, N>& a_alphaWeights,
			std::size_t a_slot,
			std::size_t& a_activeSlotCount)
		{
			for (std::size_t slot = a_slot; slot + 1 < a_activeSlotCount; ++slot) {
				a_alphaFormIDs[slot] = a_alphaFormIDs[slot + 1];
				a_alphaWeights[slot] = a_alphaWeights[slot + 1];
			}
			--a_activeSlotCount;
			a_alphaFormIDs[a_activeSlotCount] = FormID{};
			a_alphaWeights[a_activeSlotCount].fill(0.0f);
		}

		template <std::size_t N>
		void CollapseDuplicateRuntimeAlphaSlots(
			FormID a_baseFormID,
			std::array<FormID, N>& a_alphaFormIDs,
			std::array<std::array<float, LandInfo::QuadrantVertexCount>, N>& a_alphaWeights,
			std::size_t& a_activeSlotCount)
		{
			for (std::size_t slot = 0; slot < a_activeSlotCount;) {
				const auto formID = a_alphaFormIDs[slot];
				if (formID.IsNull() || formID == a_baseFormID) {
					RemoveRuntimeAlphaSlot(a_alphaFormIDs, a_alphaWeights, slot, a_activeSlotCount);
					continue;
				}

				for (std::size_t duplicate = slot + 1; duplicate < a_activeSlotCount;) {
					if (a_alphaFormIDs[duplicate] != formID) {
						++duplicate;
						continue;
					}
					for (std::size_t sample = 0; sample < LandInfo::QuadrantVertexCount; ++sample) {
						a_alphaWeights[slot][sample] = Clamp01(a_alphaWeights[slot][sample] + a_alphaWeights[duplicate][sample]);
					}
					RemoveRuntimeAlphaSlot(a_alphaFormIDs, a_alphaWeights, duplicate, a_activeSlotCount);
				}
				++slot;
			}
		}

		using LandTextureSwaps = std::unordered_map<FormID, FormID, FormIDHash>;

		[[nodiscard]] FormID SwapLandTexture(FormID a_landTexture, const LandTextureSwaps* a_swaps)
		{
			if (!a_swaps) return a_landTexture;
			const auto it = a_swaps->find(a_landTexture);
			return it != a_swaps->end() ? it->second : a_landTexture;
		}

		[[nodiscard]] FinalizedTerrainQuadrantLayers BuildSkyrimCloseTerrainLayers(const LandInfo& a_land, std::uint8_t a_quadrant, const LandTextureSwaps* a_swaps)
		{
			FinalizedTerrainQuadrantLayers finalized;
			FormID baseFormID = SwapLandTexture(a_land.baseTextures[a_quadrant]
				? ResolveDefaultLandTexture(a_land.baseTextures[a_quadrant]->landTextureFormID)
				: kSkyrimDefaultLandTextureFormID, a_swaps);

			std::array<FormID, 6> alphaFormIDs{};
			std::array<std::array<float, LandInfo::QuadrantVertexCount>, 6> alphaWeights{};
			for (auto& weights : alphaWeights) {
				weights.fill(0.0f);
			}

			for (const auto& alpha : a_land.alphaTextures) {
				if (alpha.quadrant != a_quadrant || alpha.layerIndex >= alphaFormIDs.size()) {
					continue;
				}
				alphaFormIDs[alpha.layerIndex] = SwapLandTexture(ResolveDefaultLandTexture(alpha.landTextureFormID), a_swaps);
			}

			for (const auto& alpha : a_land.vertexAlphas) {
				if (alpha.quadrant != a_quadrant ||
					alpha.layerIndex >= alphaWeights.size() ||
					alpha.position >= LandInfo::QuadrantVertexCount) {
					continue;
				}
				alphaWeights[alpha.layerIndex][alpha.position] = NormalizeSkyrimLandOpacity(alpha.opacity);
			}

			std::size_t activeSlotCount = alphaFormIDs.size();
			CollapseDuplicateRuntimeAlphaSlots(baseFormID, alphaFormIDs, alphaWeights, activeSlotCount);

			finalized.layerFormIDs[0] = baseFormID;
			for (std::size_t slot = 0; slot < activeSlotCount; ++slot) {
				finalized.layerFormIDs[slot + 1] = alphaFormIDs[slot];
				finalized.weights[slot + 1] = alphaWeights[slot];
			}

			for (std::size_t sample = 0; sample < LandInfo::QuadrantVertexCount; ++sample) {
				float alphaCoverage = 0.0f;
				for (std::size_t slot = 1; slot < finalized.weights.size(); ++slot) {
					alphaCoverage += finalized.weights[slot][sample];
				}
				finalized.weights[0][sample] = Clamp01(1.0f - alphaCoverage);
			}

			return finalized;
		}

		using TextureSetMap = std::unordered_map<FormID, TextureSetInfo, FormIDHash>;
		using LandTextureMap = std::unordered_map<FormID, LandTextureInfo, FormIDHash>;

		[[nodiscard]] const TextureSetInfo* ResolveLandTextureSet(
			FormID a_landTextureFormID,
			const TextureSetMap& a_textureSets,
			const LandTextureMap& a_landTextures,
			const LandTextureInfo** a_landTexture)
		{
			if (a_landTexture) {
				*a_landTexture = nullptr;
			}
			const auto landTextureIt = a_landTextures.find(a_landTextureFormID);
			if (landTextureIt == a_landTextures.end()) {
				return nullptr;
			}
			if (a_landTexture) {
				*a_landTexture = std::addressof(landTextureIt->second);
			}
			const auto textureSetIt = a_textureSets.find(landTextureIt->second.textureSetFormID);
			return textureSetIt == a_textureSets.end() ? nullptr : std::addressof(textureSetIt->second);
		}

		void BuildTerrainGeometry(const std::vector<LandInfo>& lands, TerrainWorldBuildResult& result)
		{
		result.vertices.reserve(lands.size() * LandInfo::VertexCount);
		result.indices.reserve(lands.size() * 32u * 32u * 6u);

		std::unordered_map<TerrainLatticeKey, std::uint32_t, TerrainLatticeKeyHash> vertexByLattice;
		vertexByLattice.reserve(lands.size() * LandInfo::VertexCount);

		for (const auto& land : lands) {
			if (!land.hasHeights || !land.cellX || !land.cellY) {
				continue;
			}

			std::array<std::uint32_t, LandInfo::VertexCount> cellIndices{};
			for (std::size_t y = 0; y < LandInfo::VertexSide; ++y) {
				for (std::size_t x = 0; x < LandInfo::VertexSide; ++x) {
					const auto localIndex = y * LandInfo::VertexSide + x;
					const TerrainLatticeKey key{
						.x = *land.cellX * 32 + static_cast<std::int32_t>(x),
						.y = *land.cellY * 32 + static_cast<std::int32_t>(y),
					};
					const auto [it, inserted] = vertexByLattice.emplace(key, static_cast<std::uint32_t>(result.vertices.size()));
					if (inserted) {
						TerrainVertex vertex{};
						vertex.position[0] = static_cast<float>(*land.cellX * 4096 + static_cast<std::int32_t>(x) * 128);
						vertex.position[1] = static_cast<float>(*land.cellY * 4096 + static_cast<std::int32_t>(y) * 128);
						vertex.position[2] = land.heights[localIndex];
						vertex.uv[0] = vertex.position[0] / 4096.0f;
						vertex.uv[1] = vertex.position[1] / 4096.0f;
						if (land.hasVertexColors) {
							vertex.color[0] = static_cast<float>(land.vertexColors[localIndex][0]) / 255.0f;
							vertex.color[1] = static_cast<float>(land.vertexColors[localIndex][1]) / 255.0f;
							vertex.color[2] = static_cast<float>(land.vertexColors[localIndex][2]) / 255.0f;
						}
						vertex.normal[0] = 0.0f;
						vertex.normal[1] = 0.0f;
						vertex.normal[2] = 0.0f;
						result.vertices.push_back(vertex);
					}
					cellIndices[localIndex] = it->second;
				}
			}

			for (std::size_t y = 0; y < 32; ++y) {
				for (std::size_t x = 0; x < 32; ++x) {
					const auto i00 = cellIndices[y * LandInfo::VertexSide + x];
					const auto i10 = cellIndices[y * LandInfo::VertexSide + x + 1];
					const auto i01 = cellIndices[(y + 1) * LandInfo::VertexSide + x];
					const auto i11 = cellIndices[(y + 1) * LandInfo::VertexSide + x + 1];
					result.indices.insert(result.indices.end(), { i00, i10, i01, i10, i11, i01 });
				}
			}
		}

		for (auto& vertex : result.vertices) {
			vertex.normal[0] = 0.0f;
			vertex.normal[1] = 0.0f;
			vertex.normal[2] = 0.0f;
		}
		for (std::size_t i = 0; i + 2 < result.indices.size(); i += 3) {
			AccumulateTriangleNormal(result.vertices, result.indices[i + 0], result.indices[i + 1], result.indices[i + 2]);
		}
		for (auto& vertex : result.vertices) {
			const float lengthSq =
				vertex.normal[0] * vertex.normal[0] +
				vertex.normal[1] * vertex.normal[1] +
				vertex.normal[2] * vertex.normal[2];
			if (lengthSq > 0.000001f) {
				const float invLength = 1.0f / std::sqrt(lengthSq);
				vertex.normal[0] *= invLength;
				vertex.normal[1] *= invLength;
				vertex.normal[2] *= invLength;
			} else {
				vertex.normal[0] = 0.0f;
				vertex.normal[1] = 0.0f;
				vertex.normal[2] = 1.0f;
			}
		}
		}

		void BuildTerrainMaterial(
			const std::vector<LandInfo>& lands,
			const TextureSetMap& a_textureSets,
			const LandTextureMap& a_landTextures,
			const LandTextureSwaps* a_swaps,
			TerrainWorldBuildResult& result)
		{
		TW_ZONE("BuildTerrainMaterial");
		result.layers.push_back(TerrainLayer{
			.landTextureFormID = FormID{},
			.textureSetFormID = FormID{},
			.textureSetEditorID = {},
			.diffusePath = "textures\\_land_default.dds",
			.normalPath = {},
			.heightPath = {},
			.rmaosPath = {},
			.uvScale = kSkyrimDefaultTerrainLayerUvScale,
			.materialFlags = 0,
		});
		std::unordered_map<FormID, std::uint32_t, FormIDHash> terrainLayerByLandTexture;
		terrainLayerByLandTexture.emplace(FormID{}, 0);
		const auto getLayerIndex = [&](FormID landTextureFormID) -> std::uint32_t {
			if (const auto it = terrainLayerByLandTexture.find(landTextureFormID); it != terrainLayerByLandTexture.end()) {
				return it->second;
			}
			const LandTextureInfo* landTexture = nullptr;
			const auto* textureSet = ResolveLandTextureSet(landTextureFormID, a_textureSets, a_landTextures, std::addressof(landTexture));
			const auto index = static_cast<std::uint32_t>(result.layers.size());
			result.layers.push_back(TerrainLayer{
				.landTextureFormID = landTextureFormID,
				.textureSetFormID = landTexture ? landTexture->textureSetFormID : FormID{},
				.textureSetEditorID = textureSet ? textureSet->editorID : std::string{},
				.diffusePath = textureSet ? textureSet->texturePaths[0] : std::string{},
				.normalPath = textureSet ? textureSet->texturePaths[1] : std::string{},
				.heightPath = textureSet ? textureSet->texturePaths[4] : std::string{},
				.rmaosPath = textureSet ? textureSet->texturePaths[5] : std::string{},
				.uvScale = kSkyrimDefaultTerrainLayerUvScale,
				.materialFlags = landTexture ? landTexture->materialFlags : 0u,
			});
			terrainLayerByLandTexture.emplace(landTextureFormID, index);
			return index;
		};

		// Each quadrant's own layers and their weights on its 17x17 lattice.
		std::unordered_map<TerrainQuadrantPaintKey, FinalizedTerrainQuadrantLayers, TerrainQuadrantPaintKeyHash> finalizedLayersByQuadrant;
		finalizedLayersByQuadrant.reserve(lands.size() * LandInfo::QuadrantCount);
		{
			TW_ZONE("terrain quadrant layers");
			std::vector<FinalizedTerrainQuadrantLayers> built(lands.size() * LandInfo::QuadrantCount);
			oneapi::tbb::parallel_for(std::size_t{ 0 }, lands.size(), [&](std::size_t i) {
				if (!lands[i].cellX || !lands[i].cellY) return;
				TW_ZONE("LAND's quadrant layers");
				for (std::uint8_t quadrant = 0; quadrant < LandInfo::QuadrantCount; ++quadrant) {
					built[i * LandInfo::QuadrantCount + quadrant] = BuildSkyrimCloseTerrainLayers(lands[i], quadrant, a_swaps);
				}
			});
			for (std::size_t i = 0; i < lands.size(); ++i) {
				if (!lands[i].cellX || !lands[i].cellY) continue;
				for (std::uint8_t quadrant = 0; quadrant < LandInfo::QuadrantCount; ++quadrant) {
					finalizedLayersByQuadrant.emplace(
						TerrainQuadrantPaintKey{ .cellX = *lands[i].cellX, .cellY = *lands[i].cellY, .quadrant = quadrant },
						std::move(built[i * LandInfo::QuadrantCount + quadrant]));
				}
			}
		}

		const auto floorDiv = [](std::int32_t a_value, std::int32_t a_divisor) {
			const auto quotient = a_value / a_divisor;
			return (a_value % a_divisor != 0 && ((a_value < 0) != (a_divisor < 0))) ? quotient - 1 : quotient;
		};

		// A lattice point's painted layers: each layer's strongest weight among the quadrants whose lattice holds the
		// point (a point on a quadrant's edge is on its neighbour's too), divided by the layers' sum. Computed where a
		// region needs it, rather than kept for the whole worldspace.
		struct SampleLayers
		{
			std::array<std::pair<FormID, float>, 28> layers{};
			std::size_t count{ 0 };

			[[nodiscard]] float Weight(FormID a_layer) const
			{
				for (std::size_t i = 0; i < count; ++i) {
					if (layers[i].first == a_layer) return layers[i].second;
				}
				return 0.0f;
			}
		};
		const auto sampleLayers = [&](std::int32_t a_x, std::int32_t a_y) {
			SampleLayers sample;
			const auto qx = floorDiv(a_x, 16);
			const auto qy = floorDiv(a_y, 16);
			const std::int32_t xs[2] = { qx, qx - 1 };
			const std::int32_t ys[2] = { qy, qy - 1 };
			const int xCount = (a_x - qx * 16) == 0 ? 2 : 1;
			const int yCount = (a_y - qy * 16) == 0 ? 2 : 1;
			for (int iy = 0; iy < yCount; ++iy) {
				for (int ix = 0; ix < xCount; ++ix) {
					const auto cqx = xs[ix], cqy = ys[iy];
					const auto cellX = floorDiv(cqx, 2), cellY = floorDiv(cqy, 2);
					const auto quadrant = static_cast<std::uint8_t>((cqx - cellX * 2) | ((cqy - cellY * 2) << 1));
					const auto it = finalizedLayersByQuadrant.find(TerrainQuadrantPaintKey{ .cellX = cellX, .cellY = cellY, .quadrant = quadrant });
					if (it == finalizedLayersByQuadrant.end()) continue;
					const auto local = static_cast<std::size_t>(a_y - cqy * 16) * LandInfo::QuadrantVertexSide + static_cast<std::size_t>(a_x - cqx * 16);
					for (std::size_t slot = 0; slot < it->second.layerFormIDs.size(); ++slot) {
						const auto layer = it->second.layerFormIDs[slot];
						const auto weight = it->second.weights[slot][local];
						if (layer.IsNull() || weight <= 0.0f) continue;
						bool found = false;
						for (std::size_t i = 0; i < sample.count; ++i) {
							if (sample.layers[i].first == layer) {
								sample.layers[i].second = (std::max)(sample.layers[i].second, weight);
								found = true;
								break;
							}
						}
						if (!found && sample.count < sample.layers.size()) sample.layers[sample.count++] = { layer, weight };
					}
				}
			}
			float sum = 0.0f;
			for (std::size_t i = 0; i < sample.count; ++i) sum += sample.layers[i].second;
			if (sum <= 1.0e-5f) {
				sample.count = 0;
				return sample;
			}
			for (std::size_t i = 0; i < sample.count; ++i) sample.layers[i].second = Clamp01(sample.layers[i].second / sum);
			return sample;
		};

		constexpr std::int32_t gutter = 1;
		constexpr std::uint32_t blockSide = static_cast<std::uint32_t>(LandInfo::QuadrantVertexSide + gutter * 2);
		constexpr std::uint32_t samplesPerRegion = blockSide * blockSide;
		constexpr std::uint32_t packedWordsPerLayer = (samplesPerRegion + 3u) / 4u;

		// Each region (quadrant) on its own, in parallel: its layers (its own, then the ones its window's samples paint,
		// strongest first) and their weights packed a byte per sample.
		struct RegionBuild
		{
			bool present{ false };
			TerrainRegionDescriptor descriptor{};
			std::vector<FormID> layers;
			std::vector<std::uint32_t> words;
			std::uint32_t supportLayers{ 0 };
		};
		std::vector<RegionBuild> regionBuilds(lands.size() * LandInfo::QuadrantCount);
		oneapi::tbb::parallel_for(std::size_t{ 0 }, regionBuilds.size(), [&](std::size_t index) {
			const auto& land = lands[index / LandInfo::QuadrantCount];
			const auto quadrant = static_cast<std::uint8_t>(index % LandInfo::QuadrantCount);
			if (!land.cellX || !land.cellY) return;
			TW_ZONE("terrain region");
			const auto finalizedIt = finalizedLayersByQuadrant.find(TerrainQuadrantPaintKey{ .cellX = *land.cellX, .cellY = *land.cellY, .quadrant = quadrant });
			if (finalizedIt == finalizedLayersByQuadrant.end()) return;
			const auto& closeLayers = finalizedIt->second;
			auto& build = regionBuilds[index];
			build.present = true;

			const auto addRegionLayer = [&](FormID layerFormID) {
				if (layerFormID.IsNull() || std::ranges::find(build.layers, layerFormID) != build.layers.end()) return;
				build.layers.push_back(layerFormID);
			};
			for (std::size_t i = 0; i < closeLayers.layerFormIDs.size(); ++i) {
				const bool hasWeight = std::ranges::any_of(closeLayers.weights[i], [](float weight) { return weight > 0.0f; });
				if (i == 0 || (!closeLayers.layerFormIDs[i].IsNull() && hasWeight)) addRegionLayer(closeLayers.layerFormIDs[i]);
			}

			const auto globalBaseX = *land.cellX * 32 + ((quadrant & 1u) ? 16 : 0);
			const auto globalBaseY = *land.cellY * 32 + ((quadrant & 2u) ? 16 : 0);
			std::array<SampleLayers, samplesPerRegion> window;
			std::vector<std::pair<FormID, float>> support;
			for (std::int32_t y = -gutter; y < static_cast<std::int32_t>(LandInfo::QuadrantVertexSide) + gutter; ++y) {
				for (std::int32_t x = -gutter; x < static_cast<std::int32_t>(LandInfo::QuadrantVertexSide) + gutter; ++x) {
					auto& sample = window[static_cast<std::size_t>(y + gutter) * blockSide + static_cast<std::size_t>(x + gutter)];
					sample = sampleLayers(globalBaseX + x, globalBaseY + y);
					for (std::size_t i = 0; i < sample.count; ++i) {
						const auto [layer, weight] = sample.layers[i];
						auto it = std::ranges::find_if(support, [&](const auto& entry) { return entry.first == layer; });
						if (it == support.end()) {
							support.emplace_back(layer, weight);
						} else {
							it->second += weight;
						}
					}
				}
			}
			std::ranges::sort(support, [](const auto& lhs, const auto& rhs) {
				if (lhs.second != rhs.second) return lhs.second > rhs.second;
				return lhs.first.value < rhs.first.value;
			});
			build.supportLayers = static_cast<std::uint32_t>(support.size());
			for (const auto& [layer, _] : support) addRegionLayer(layer);

			build.descriptor.regionX = *land.cellX * 2 + ((quadrant & 1u) ? 1 : 0);
			build.descriptor.regionY = *land.cellY * 2 + ((quadrant & 2u) ? 1 : 0);
			build.descriptor.weightSampleSide = blockSide;
			build.words.assign(build.layers.size() * packedWordsPerLayer, 0u);
			for (std::size_t sampleIndex = 0; sampleIndex < samplesPerRegion; ++sampleIndex) {
				for (std::size_t localLayer = 0; localLayer < build.layers.size(); ++localLayer) {
					PackWeightByte(build.words, localLayer * packedWordsPerLayer, sampleIndex, window[sampleIndex].Weight(build.layers[localLayer]));
				}
			}
		});

		TW_ZONE("terrain regions packed");
		result.regions.reserve(regionBuilds.size());
		for (auto& build : regionBuilds) {
			if (!build.present) continue;
			result.diagnostics.maxFilterFootprintLayers = (std::max)(result.diagnostics.maxFilterFootprintLayers, build.supportLayers);
			result.diagnostics.totalFilterFootprintLayers += build.supportLayers;
			auto descriptor = build.descriptor;
			descriptor.layerRefStart = static_cast<std::uint32_t>(result.layerRefs.size());
			descriptor.layerRefCount = static_cast<std::uint32_t>(build.layers.size());
			descriptor.weightBlockStart = static_cast<std::uint32_t>(result.weightBlocks.size());
			for (const auto layerFormID : build.layers) {
				result.layerRefs.push_back(TerrainLayerRef{ .layerIndex = getLayerIndex(layerFormID) });
			}
			result.weightBlocks.insert(result.weightBlocks.end(), build.words.begin(), build.words.end());
			result.regions.push_back(descriptor);
		}
		}

		TerrainWorldBuildResult BuildTerrainWorldFrom(
			FormID a_worldFormID,
			const std::unordered_map<FormID, std::vector<LandInfo>, FormIDHash>& a_landsByWorldspace,
			const TextureSetMap& a_textureSets,
			const LandTextureMap& a_landTextures,
			TerrainBuildOptions a_options)
		{
			TW_ZONE("BuildTerrainWorld");
			TerrainWorldBuildResult result;
			result.worldFormID = a_worldFormID;
			const auto landsIt = a_landsByWorldspace.find(a_worldFormID);
			if (landsIt == a_landsByWorldspace.end()) {
				return result;
			}
			if (a_options.geometry) {
				BuildTerrainGeometry(landsIt->second, result);
			}
			if (a_options.material) {
				BuildTerrainMaterial(landsIt->second, a_textureSets, a_landTextures, a_options.landTextureSwaps, result);
			}
			return result;
		}
	}

	TerrainWorldBuildResult BuildTerrainWorld(FormID a_worldFormID, const WorldIndex& a_worldIndex, TerrainBuildOptions a_options)
	{
		return BuildTerrainWorldFrom(a_worldFormID, a_worldIndex.landsByWorldspace, a_worldIndex.textureSets, a_worldIndex.landTextures, a_options);
	}

	TerrainWorldBuildResult BuildTerrainWorld(FormID a_worldFormID, const StaticWorldSnapshot& a_snapshot, TerrainBuildOptions a_options)
	{
		return BuildTerrainWorldFrom(
			a_worldFormID,
			a_snapshot.landsByWorldspace,
			a_snapshot.textureSetsByFormID,
			a_snapshot.landTexturesByFormID,
			a_options);
	}

	void CellInfo::Apply(const CellInfo& a_partial)
	{
		flags = a_partial.flags;
		if (a_partial.worldFormID) worldFormID = a_partial.worldFormID;
		if (a_partial.fields & FieldEditorID) editorID = a_partial.editorID;
		if (a_partial.fields & FieldData) cellFlags = a_partial.cellFlags;
		if (a_partial.fields & FieldGrid) {
			gridX = a_partial.gridX;
			gridY = a_partial.gridY;
			forceHideLand = a_partial.forceHideLand;
		}
		if (a_partial.fields & FieldWater) waterHeight = a_partial.waterHeight;
		fields |= a_partial.fields;
	}

	void WorldInfo::Apply(const WorldInfo& a_partial)
	{
		flags = a_partial.flags;
		if (a_partial.fields & FieldEditorID) editorID = a_partial.editorID;
		if (a_partial.fields & FieldParent) parentWorld = a_partial.parentWorld;
		if (a_partial.fields & FieldParentUse) parentUseFlags = a_partial.parentUseFlags;
		if (a_partial.fields & FieldData) dataFlags = a_partial.dataFlags;
		if (a_partial.fields & FieldHeights) {
			defaultLandHeight = a_partial.defaultLandHeight;
			defaultWaterHeight = a_partial.defaultWaterHeight;
		}
		if (a_partial.fields & FieldBoundsMin) boundsMin = a_partial.boundsMin;
		if (a_partial.fields & FieldBoundsMax) boundsMax = a_partial.boundsMax;
		if (a_partial.fields & FieldLodWaterType) lodWaterType = a_partial.lodWaterType;
		if (a_partial.fields & FieldLodWaterHeight) lodWaterHeight = a_partial.lodWaterHeight;
		if (a_partial.fields & FieldLodMultiplier) distantLodMultiplier = a_partial.distantLodMultiplier;
		if (a_partial.fields & FieldHdLodDiffuse) hdLodDiffuse = a_partial.hdLodDiffuse;
		if (a_partial.fields & FieldHdLodNormal) hdLodNormal = a_partial.hdLodNormal;
		if (a_partial.fields & FieldLargeReferences) largeReferences = a_partial.largeReferences;
		fields |= a_partial.fields;
	}

	const std::vector<PlacementInfo>* StaticWorldSnapshot::GetExteriorCell(FormID a_worldFormID, std::int32_t a_x, std::int32_t a_y) const
	{
		const auto it = exteriorPlacementsByCell.find(CellKey{ a_worldFormID, a_x, a_y });
		return it == exteriorPlacementsByCell.end() ? nullptr : std::addressof(it->second);
	}

	const std::vector<PlacementInfo>* StaticWorldSnapshot::GetInteriorCell(FormID a_cellFormID) const
	{
		const auto it = interiorPlacementsByCell.find(a_cellFormID);
		return it == interiorPlacementsByCell.end() ? nullptr : std::addressof(it->second);
	}

	WorldIndex StaticWorldSnapshot::MaterializeWorldIndex() const
	{
		WorldIndex index;
		index.baseObjects = baseObjectsByFormID;
		index.textureSets = textureSetsByFormID;
		index.landTextures = landTexturesByFormID;
		index.grasses = grassesByFormID;
		index.cells = cellsByFormID;
		index.landsByWorldspace = landsByWorldspace;
		index.exteriorPlacements = exteriorPlacementsByCell;
		index.interiorPlacements = interiorPlacementsByCell;
		index.orphanPlacements = orphanPlacements;
		return index;
	}

	StaticWorldSnapshot BuildStaticWorldSnapshot(
		const std::vector<StaticPluginShard>& a_shards,
		StaticWorldSnapshotBuildOptions a_options)
	{
		StaticWorldSnapshot snapshot;

		auto phaseBegin = Clock::now();
		std::size_t baseCount = 0;
		std::size_t textureSetCount = 0;
		std::size_t landTextureCount = 0;
		std::size_t grassCount = 0;
		std::size_t worldCount = 0;
		std::size_t cellCount = 0;
		std::size_t landCount = 0;
		std::size_t placementCount = 0;
		std::size_t suppressorCount = 0;
		for (const auto& shard : a_shards) {
			baseCount += shard.baseObjects.size();
			textureSetCount += shard.textureSets.size();
			landTextureCount += shard.landTextures.size();
			grassCount += shard.grasses.size();
			worldCount += shard.worlds.size();
			cellCount += shard.cells.size();
			landCount += shard.lands.size();
			placementCount += shard.placements.size();
			suppressorCount += shard.suppressors.size();
		}
		snapshot.buildStats.countInputsMilliseconds = ElapsedMilliseconds(phaseBegin);
		snapshot.buildStats.placementCandidates = placementCount;

		phaseBegin = Clock::now();
		snapshot.baseObjectsByFormID.reserve(baseCount);
		snapshot.textureSetsByFormID.reserve(textureSetCount);
		snapshot.landTexturesByFormID.reserve(landTextureCount);
		snapshot.grassesByFormID.reserve(grassCount);
		snapshot.worldsByFormID.reserve(worldCount);
		snapshot.cellsByFormID.reserve(cellCount);

		// The first definition of each form: its creation order and editor ID. Forms are created in load order; a later
		// plugin's override keeps the original's place.
		const auto recordFirstDefinitions = [&](const auto& a_records) {
			for (const auto& record : a_records) {
				if (record.formID.IsNull() || record.formID.IsNone()) continue;
				const auto order = static_cast<std::uint32_t>(snapshot.creationOrder.size());
				if (snapshot.creationOrder.try_emplace(record.formID, order).second && !record.editorID.empty()) {
					snapshot.firstEditorIDs.emplace(record.formID, record.editorID);
				}
			}
		};

		std::unordered_map<FormID, std::vector<LargeReference>, FormIDHash> largeReferencesByWorld;
		for (const auto& shard : a_shards) {
			recordFirstDefinitions(shard.landTextures);
			recordFirstDefinitions(shard.materialTypes);
			recordFirstDefinitions(shard.textureSets);
			recordFirstDefinitions(shard.materialObjects);
			recordFirstDefinitions(shard.baseObjects);
			const auto isMaster =
				(shard.header.flags & TES4_FLAG_ESM) != 0 || shard.entry.pluginName.ends_with(".esm") || shard.entry.pluginName.ends_with(".esl");
			for (const auto& world : shard.worlds) {
				if (!world.formID.IsNull() && !world.formID.IsNone()) {
                    if (a_options.enginePartialForms && world.IsPartialForm()) {
                        if (const auto it = snapshot.worldsByFormID.find(world.formID); it != snapshot.worldsByFormID.end()) {
                            if (world.fields & WorldInfo::FieldBoundsMin) it->second.boundsMin = world.boundsMin;
                            if (world.fields & WorldInfo::FieldBoundsMax) it->second.boundsMax = world.boundsMax;
                        }
                        continue;
                    }
					if (const auto it = snapshot.worldsByFormID.find(world.formID); it != snapshot.worldsByFormID.end() && world.IsPartialForm()) {
						it->second.Apply(world);
					} else {
						snapshot.worldsByFormID.insert_or_assign(world.formID, world);
					}
					if (isMaster) {
						auto& merged = largeReferencesByWorld[world.formID];
						merged.insert(merged.end(), world.largeReferences.begin(), world.largeReferences.end());
					}
				}
			}
			for (const auto& material : shard.materialObjects) {
				if (!material.formID.IsNull() && !material.formID.IsNone()) {
					snapshot.materialObjectsByFormID.insert_or_assign(material.formID, material.editorID);
				}
			}
			for (const auto& region : shard.regions)
				if (!region.formID.IsNull() && !region.formID.IsNone()) snapshot.regionsByFormID.insert_or_assign(region.formID, region.editorID);
			for (const auto& material : shard.materialTypes) {
				if (!material.formID.IsNull() && !material.formID.IsNone()) {
					snapshot.materialTypesByFormID.insert_or_assign(material.formID, material);
				}
			}
			for (const auto& base : shard.baseObjects) {
				if (!base.formID.IsNull() && !base.formID.IsNone()) {
					snapshot.baseObjectsByFormID.insert_or_assign(base.formID, base);
				}
			}
			for (const auto& textureSet : shard.textureSets) {
				if (!textureSet.formID.IsNull() && !textureSet.formID.IsNone()) {
					snapshot.textureSetsByFormID.insert_or_assign(textureSet.formID, textureSet);
				}
			}
			for (const auto& landTexture : shard.landTextures) {
				if (!landTexture.formID.IsNull() && !landTexture.formID.IsNone()) {
					snapshot.landTexturesByFormID.insert_or_assign(landTexture.formID, landTexture);
				}
			}
			for (const auto& grass : shard.grasses) {
				if (!grass.formID.IsNull() && !grass.formID.IsNone()) {
					snapshot.grassesByFormID.insert_or_assign(grass.formID, grass);
				}
			}
			for (const auto& cell : shard.cells) {
				if (!cell.formID.IsNull() && !cell.formID.IsNone()) {
                    if (a_options.enginePartialForms && cell.IsPartialForm()) continue;
					if (const auto it = snapshot.cellsByFormID.find(cell.formID); it != snapshot.cellsByFormID.end() && cell.IsPartialForm()) {
						it->second.Apply(cell);
					} else {
						snapshot.cellsByFormID.insert_or_assign(cell.formID, cell);
					}
				}
			}
			for (const auto& suppressor : shard.suppressors) {
				++snapshot.suppressedRecords;
				if ((suppressor.flags & (1u << 12)) != 0) continue;
				if (IsBaseObjectSignature(suppressor.signature)) {
					snapshot.baseObjectsByFormID.erase(suppressor.formID);
				} else if (suppressor.signature == SIG_TXST) {
					snapshot.textureSetsByFormID.erase(suppressor.formID);
				} else if (suppressor.signature == SIG_LTEX) {
					snapshot.landTexturesByFormID.erase(suppressor.formID);
				} else if (suppressor.signature == SIG_GRAS) {
					snapshot.grassesByFormID.erase(suppressor.formID);
				} else if (suppressor.signature == SIG_WRLD) {
					snapshot.worldsByFormID.erase(suppressor.formID);
				} else if (suppressor.signature == SIG_MATO) {
					snapshot.materialObjectsByFormID.erase(suppressor.formID);
				} else if (suppressor.signature == SIG_REGN) {
					snapshot.regionsByFormID.erase(suppressor.formID);
				} else if (suppressor.signature == SIG_MATT) {
					snapshot.materialTypesByFormID.erase(suppressor.formID);
				} else if (suppressor.signature == SIG_CELL) {
					snapshot.cellsByFormID.erase(suppressor.formID);
				}
			}
		}

		for (auto& [worldFormID, world] : snapshot.worldsByFormID) {
			auto merged = largeReferencesByWorld.find(worldFormID);
			if (merged == largeReferencesByWorld.end()) {
				world.largeReferences.clear();
				continue;
			}
			auto& references = merged->second;
			std::ranges::stable_sort(references, [](const LargeReference& a, const LargeReference& b) { return a.reference < b.reference; });
			const auto duplicates = std::ranges::unique(references, [](const LargeReference& a, const LargeReference& b) { return a.reference == b.reference; });
			references.erase(duplicates.begin(), duplicates.end());
			world.largeReferences = std::move(references);
		}

		// A LandInfo is ~11 KB of fixed arrays, so a move is a copy. Select the
		// winners first, then size each worldspace once and copy every winner
		// exactly once instead of copying rejected records and regrowing vectors.
		// CELL owns a single LAND child. A later explicit child replaces that slot even if its FormID changes;
		// a CELL override without a LAND does not touch it. Deleting a displaced older LAND cannot clear its replacement.
		std::unordered_map<FormID, FormID, FormIDHash> landByCell;
		std::unordered_map<FormID, FormID, FormIDHash> cellByLand;
		for (const auto& shard : a_shards) {
			for (const auto& land : shard.lands) {
				if (const auto previous = cellByLand.find(land.formID); previous != cellByLand.end() && previous->second != land.parentCell) {
					const auto slot = landByCell.find(previous->second);
					if (slot != landByCell.end() && slot->second == land.formID) landByCell.erase(slot);
				}
				cellByLand.insert_or_assign(land.formID, land.parentCell);
				landByCell.insert_or_assign(land.parentCell, land.formID);
			}
			for (const auto& suppressor : shard.suppressors) {
				if (suppressor.signature != SIG_LAND || (suppressor.flags & (1u << 12)) != 0) continue;
				if (const auto parent = cellByLand.find(suppressor.formID); parent != cellByLand.end()) {
					const auto slot = landByCell.find(parent->second);
					if (slot != landByCell.end() && slot->second == suppressor.formID) landByCell.erase(slot);
					cellByLand.erase(parent);
				}
			}
		}
		FormIDSeenSet seenLands{ landCount + suppressorCount };
		std::vector<std::pair<const LandInfo*, const CellInfo*>> winningLands;
		winningLands.reserve(landCount);
		std::unordered_map<FormID, std::size_t, FormIDHash> landCountsByWorld;
		for (auto shardIt = a_shards.rbegin(); shardIt != a_shards.rend(); ++shardIt) {
			for (const auto& suppressor : shardIt->suppressors) {
				if (suppressor.signature == SIG_LAND && (suppressor.flags & (1u << 12)) == 0) {
					(void)seenLands.Insert(suppressor.formID);
				}
			}
			for (auto landIt = shardIt->lands.rbegin(); landIt != shardIt->lands.rend(); ++landIt) {
				if (landIt->formID.IsNull() || landIt->formID.IsNone() || !seenLands.Insert(landIt->formID)) {
					continue;
				}
				const auto slot = landByCell.find(landIt->parentCell);
				if (slot == landByCell.end() || slot->second != landIt->formID) continue;
				const auto cellIt = snapshot.cellsByFormID.find(landIt->parentCell);
				if (cellIt == snapshot.cellsByFormID.end() || !cellIt->second.worldFormID || !cellIt->second.gridX || !cellIt->second.gridY) {
					continue;
				}
				winningLands.emplace_back(std::addressof(*landIt), std::addressof(cellIt->second));
				++landCountsByWorld[*cellIt->second.worldFormID];
			}
		}
		for (const auto& [worldFormID, count] : landCountsByWorld) {
			snapshot.landsByWorldspace[worldFormID].reserve(count);
		}
		for (const auto& [source, cell] : winningLands) {
			auto& land = snapshot.landsByWorldspace[*cell->worldFormID].emplace_back(*source);
			land.worldFormID = cell->worldFormID;
			land.cellX = cell->gridX;
			land.cellY = cell->gridY;
		}
		snapshot.buildStats.mergeRecordsMilliseconds = ElapsedMilliseconds(phaseBegin);

		phaseBegin = Clock::now();
		FormIDSeenSet seenPlacements{ placementCount + suppressorCount };
		snapshot.exteriorPlacementsByCell.reserve(16000);
		snapshot.interiorPlacementsByCell.reserve(4096);

		const auto bucketPlacement = [&](PlacementInfo placement) {
			if (placement.IsInitiallyDisabled() && !a_options.includeDisabledPlacements) {
				++snapshot.buildStats.skippedDisabledPlacements;
				return;
			}
			if (!snapshot.baseObjectsByFormID.contains(placement.baseFormID)) {
				++snapshot.buildStats.skippedMissingBasePlacements;
				return;
			}
			const auto cellIt = snapshot.cellsByFormID.find(placement.parentCell);
			if (cellIt == snapshot.cellsByFormID.end()) {
				++snapshot.buildStats.skippedMissingCellPlacements;
				snapshot.orphanPlacements.push_back(std::move(placement));
				return;
			}

			const auto& cell = cellIt->second;
			placement.worldFormID = cell.worldFormID;
			if (auto exteriorKey = ResolvePlacementExteriorCellKey(cell, placement)) {
				snapshot.exteriorPlacementsByCell[*exteriorKey].push_back(std::move(placement));
			} else {
				snapshot.interiorPlacementsByCell[cell.formID].push_back(std::move(placement));
			}
			++snapshot.buildStats.winningPlacements;
		};

		if (a_options.parallelPlacementBucketing) {
			std::vector<const PlacementInfo*> winningPlacements;
			winningPlacements.reserve(placementCount);
			for (auto shardIt = a_shards.rbegin(); shardIt != a_shards.rend(); ++shardIt) {
				for (const auto& suppressor : shardIt->suppressors) {
					if ((suppressor.flags & (1u << 12)) != 0 || (suppressor.signature != SIG_REFR && suppressor.signature != SIG_ACHR)) {
						continue;
					}
					if (suppressor.formID.IsNull() || suppressor.formID.IsNone()) {
						continue;
					}
					if (seenPlacements.Insert(suppressor.formID)) {
						++snapshot.buildStats.skippedDeletedPlacements;
					}
				}

				for (auto placementIt = shardIt->placements.rbegin(); placementIt != shardIt->placements.rend(); ++placementIt) {
					if (placementIt->formID.IsNull() || placementIt->formID.IsNone()) {
						continue;
					}
					if (!seenPlacements.Insert(placementIt->formID)) {
						++snapshot.buildStats.skippedPlacementOverrides;
						continue;
					}
					winningPlacements.push_back(std::addressof(*placementIt));
				}
			}

			// Each placement's destination is decided in parallel into its own slot, then appended in winning order,
			// so every cell lists its placements in the same order as the sequential path, on every run. (Merging
			// per-thread buckets made the order depend on which thread took which range.)
			enum class BucketTarget : std::uint8_t { None, Exterior, Interior, Orphan };
			struct BucketedPlacement
			{
				BucketTarget target{ BucketTarget::None };
				CellKey exteriorKey{};
				FormID interiorCell{};
				PlacementInfo placement;
			};
			std::vector<BucketedPlacement> bucketed(winningPlacements.size());
			oneapi::tbb::enumerable_thread_specific<LocalStaticPlacementBuckets> localBuckets;
			oneapi::tbb::parallel_for(
				oneapi::tbb::blocked_range<std::size_t>(0, winningPlacements.size(), 4096),
				[&](const oneapi::tbb::blocked_range<std::size_t>& range) {
					auto& local = localBuckets.local();
					for (std::size_t i = range.begin(); i != range.end(); ++i) {
						const PlacementInfo& source = *winningPlacements[i];
						if (source.IsInitiallyDisabled() && !a_options.includeDisabledPlacements) {
							++local.disabled;
							continue;
						}
						if (!snapshot.baseObjectsByFormID.contains(source.baseFormID)) {
							++local.missingBase;
							continue;
						}
						auto& slot = bucketed[i];
						slot.placement = source;
						const auto cellIt = snapshot.cellsByFormID.find(source.parentCell);
						if (cellIt == snapshot.cellsByFormID.end()) {
							++local.missingCell;
							slot.target = BucketTarget::Orphan;
							continue;
						}

						const auto& cell = cellIt->second;
						slot.placement.worldFormID = cell.worldFormID;
						if (auto exteriorKey = ResolvePlacementExteriorCellKey(cell, slot.placement)) {
							slot.target = BucketTarget::Exterior;
							slot.exteriorKey = *exteriorKey;
						} else {
							slot.target = BucketTarget::Interior;
							slot.interiorCell = cell.formID;
						}
						++local.winning;
					}
				});

			for (auto& local : localBuckets) {
				snapshot.buildStats.winningPlacements += local.winning;
				snapshot.buildStats.skippedDisabledPlacements += local.disabled;
				snapshot.buildStats.skippedMissingBasePlacements += local.missingBase;
				snapshot.buildStats.skippedMissingCellPlacements += local.missingCell;
			}
			for (auto& slot : bucketed) {
				switch (slot.target) {
				case BucketTarget::Exterior:
					snapshot.exteriorPlacementsByCell[slot.exteriorKey].push_back(std::move(slot.placement));
					break;
				case BucketTarget::Interior:
					snapshot.interiorPlacementsByCell[slot.interiorCell].push_back(std::move(slot.placement));
					break;
				case BucketTarget::Orphan:
					snapshot.orphanPlacements.push_back(std::move(slot.placement));
					break;
				case BucketTarget::None:
					break;
				}
			}
		} else {
			for (auto shardIt = a_shards.rbegin(); shardIt != a_shards.rend(); ++shardIt) {
				for (const auto& suppressor : shardIt->suppressors) {
					if ((suppressor.flags & (1u << 12)) != 0 || (suppressor.signature != SIG_REFR && suppressor.signature != SIG_ACHR)) {
						continue;
					}
					if (suppressor.formID.IsNull() || suppressor.formID.IsNone()) {
						continue;
					}
					if (seenPlacements.Insert(suppressor.formID)) {
						++snapshot.buildStats.skippedDeletedPlacements;
					}
				}

				for (auto placementIt = shardIt->placements.rbegin(); placementIt != shardIt->placements.rend(); ++placementIt) {
					if (placementIt->formID.IsNull() || placementIt->formID.IsNone()) {
						continue;
					}
					if (!seenPlacements.Insert(placementIt->formID)) {
						++snapshot.buildStats.skippedPlacementOverrides;
						continue;
					}
					bucketPlacement(*placementIt);
				}
			}
		}
		snapshot.buildStats.bucketPlacementsMilliseconds = ElapsedMilliseconds(phaseBegin);

		phaseBegin = Clock::now();
		snapshot.exteriorCellKeys.reserve(snapshot.exteriorPlacementsByCell.size());
		for (const auto& [key, _] : snapshot.exteriorPlacementsByCell) {
			snapshot.exteriorCellKeys.push_back(key);
		}
		std::ranges::sort(snapshot.exteriorCellKeys, [](const CellKey& lhs, const CellKey& rhs) {
			if (lhs.worldFormID != rhs.worldFormID) {
				return lhs.worldFormID < rhs.worldFormID;
			}
			if (lhs.x != rhs.x) {
				return lhs.x < rhs.x;
			}
			return lhs.y < rhs.y;
		});

		snapshot.interiorCellKeys.reserve(snapshot.interiorPlacementsByCell.size());
		for (const auto& [cell, _] : snapshot.interiorPlacementsByCell) {
			snapshot.interiorCellKeys.push_back(cell);
		}
		std::ranges::sort(snapshot.interiorCellKeys);
		snapshot.buildStats.publishKeysMilliseconds = ElapsedMilliseconds(phaseBegin);
		return snapshot;
	}

	GameDataSnapshot LoadGameData(ILoadOrderSource& a_source)
	{
		auto loadOrder = PrepareLoadOrder(a_source.Load());
		PluginParser parser;
		GameDataSnapshot snapshot;
		snapshot.plugins.reserve(loadOrder.size());
		for (std::size_t i = 0; i < loadOrder.size(); ++i) {
			snapshot.plugins.push_back(parser.Parse(loadOrder[i], i, loadOrder));
		}
		snapshot.resolvedRecords = BuildResolvedRecordStore(snapshot.plugins);
		snapshot.worldIndex = BuildWorldIndex(snapshot.resolvedRecords);
		return snapshot;
	}
}