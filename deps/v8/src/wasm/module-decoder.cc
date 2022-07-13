// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/module-decoder.h"

#include "src/base/functional.h"
#include "src/base/platform/platform.h"
#include "src/base/platform/wrappers.h"
#include "src/flags/flags.h"
#include "src/init/v8.h"
#include "src/logging/counters.h"
#include "src/logging/metrics.h"
#include "src/objects/objects-inl.h"
#include "src/utils/ostreams.h"
#include "src/wasm/canonical-types.h"
#include "src/wasm/decoder.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/init-expr-interface.h"
#include "src/wasm/struct-types.h"
#include "src/wasm/wasm-constants.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-opcodes-inl.h"

namespace v8 {
namespace internal {
namespace wasm {

#define TRACE(...)                                    \
  do {                                                \
    if (FLAG_trace_wasm_decoder) PrintF(__VA_ARGS__); \
  } while (false)

namespace {

constexpr char kNameString[] = "name";
constexpr char kSourceMappingURLString[] = "sourceMappingURL";
constexpr char kCompilationHintsString[] = "compilationHints";
constexpr char kBranchHintsString[] = "metadata.code.branch_hint";
constexpr char kDebugInfoString[] = ".debug_info";
constexpr char kExternalDebugInfoString[] = "external_debug_info";

const char* ExternalKindName(ImportExportKindCode kind) {
  switch (kind) {
    case kExternalFunction:
      return "function";
    case kExternalTable:
      return "table";
    case kExternalMemory:
      return "memory";
    case kExternalGlobal:
      return "global";
    case kExternalTag:
      return "tag";
  }
  return "unknown";
}

}  // namespace

const char* SectionName(SectionCode code) {
  switch (code) {
    case kUnknownSectionCode:
      return "Unknown";
    case kTypeSectionCode:
      return "Type";
    case kImportSectionCode:
      return "Import";
    case kFunctionSectionCode:
      return "Function";
    case kTableSectionCode:
      return "Table";
    case kMemorySectionCode:
      return "Memory";
    case kGlobalSectionCode:
      return "Global";
    case kExportSectionCode:
      return "Export";
    case kStartSectionCode:
      return "Start";
    case kCodeSectionCode:
      return "Code";
    case kElementSectionCode:
      return "Element";
    case kDataSectionCode:
      return "Data";
    case kTagSectionCode:
      return "Tag";
    case kDataCountSectionCode:
      return "DataCount";
    case kNameSectionCode:
      return kNameString;
    case kSourceMappingURLSectionCode:
      return kSourceMappingURLString;
    case kDebugInfoSectionCode:
      return kDebugInfoString;
    case kExternalDebugInfoSectionCode:
      return kExternalDebugInfoString;
    case kCompilationHintsSectionCode:
      return kCompilationHintsString;
    case kBranchHintsSectionCode:
      return kBranchHintsString;
    default:
      return "<unknown>";
  }
}

namespace {

bool validate_utf8(Decoder* decoder, WireBytesRef string) {
  return unibrow::Utf8::ValidateEncoding(
      decoder->start() + decoder->GetBufferRelativeOffset(string.offset()),
      string.length());
}

// Reads a length-prefixed string, checking that it is within bounds. Returns
// the offset of the string, and the length as an out parameter.
WireBytesRef consume_string(Decoder* decoder, bool validate_utf8,
                            const char* name) {
  uint32_t length = decoder->consume_u32v("string length");
  uint32_t offset = decoder->pc_offset();
  const byte* string_start = decoder->pc();
  // Consume bytes before validation to guarantee that the string is not oob.
  if (length > 0) {
    decoder->consume_bytes(length, name);
    if (decoder->ok() && validate_utf8 &&
        !unibrow::Utf8::ValidateEncoding(string_start, length)) {
      decoder->errorf(string_start, "%s: no valid UTF-8 string", name);
    }
  }
  return {offset, decoder->failed() ? 0 : length};
}

namespace {
SectionCode IdentifyUnknownSectionInternal(Decoder* decoder) {
  WireBytesRef string = consume_string(decoder, true, "section name");
  if (decoder->failed()) {
    return kUnknownSectionCode;
  }
  const byte* section_name_start =
      decoder->start() + decoder->GetBufferRelativeOffset(string.offset());

  TRACE("  +%d  section name        : \"%.*s\"\n",
        static_cast<int>(section_name_start - decoder->start()),
        string.length() < 20 ? string.length() : 20, section_name_start);

  using SpecialSectionPair = std::pair<base::Vector<const char>, SectionCode>;
  static constexpr SpecialSectionPair kSpecialSections[]{
      {base::StaticCharVector(kNameString), kNameSectionCode},
      {base::StaticCharVector(kSourceMappingURLString),
       kSourceMappingURLSectionCode},
      {base::StaticCharVector(kCompilationHintsString),
       kCompilationHintsSectionCode},
      {base::StaticCharVector(kBranchHintsString), kBranchHintsSectionCode},
      {base::StaticCharVector(kDebugInfoString), kDebugInfoSectionCode},
      {base::StaticCharVector(kExternalDebugInfoString),
       kExternalDebugInfoSectionCode}};

  auto name_vec = base::Vector<const char>::cast(
      base::VectorOf(section_name_start, string.length()));
  for (auto& special_section : kSpecialSections) {
    if (name_vec == special_section.first) return special_section.second;
  }

  return kUnknownSectionCode;
}
}  // namespace

// An iterator over the sections in a wasm binary module.
// Automatically skips all unknown sections.
class WasmSectionIterator {
 public:
  explicit WasmSectionIterator(Decoder* decoder)
      : decoder_(decoder),
        section_code_(kUnknownSectionCode),
        section_start_(decoder->pc()),
        section_end_(decoder->pc()) {
    next();
  }

  bool more() const { return decoder_->ok() && decoder_->more(); }

  SectionCode section_code() const { return section_code_; }

  const byte* section_start() const { return section_start_; }

  uint32_t section_length() const {
    return static_cast<uint32_t>(section_end_ - section_start_);
  }

  base::Vector<const uint8_t> payload() const {
    return {payload_start_, payload_length()};
  }

  const byte* payload_start() const { return payload_start_; }

  uint32_t payload_length() const {
    return static_cast<uint32_t>(section_end_ - payload_start_);
  }

  const byte* section_end() const { return section_end_; }

  // Advances to the next section, checking that decoding the current section
  // stopped at {section_end_}.
  void advance(bool move_to_section_end = false) {
    if (move_to_section_end && decoder_->pc() < section_end_) {
      decoder_->consume_bytes(
          static_cast<uint32_t>(section_end_ - decoder_->pc()));
    }
    if (decoder_->pc() != section_end_) {
      const char* msg = decoder_->pc() < section_end_ ? "shorter" : "longer";
      decoder_->errorf(decoder_->pc(),
                       "section was %s than expected size "
                       "(%u bytes expected, %zu decoded)",
                       msg, section_length(),
                       static_cast<size_t>(decoder_->pc() - section_start_));
    }
    next();
  }

 private:
  Decoder* decoder_;
  SectionCode section_code_;
  const byte* section_start_;
  const byte* payload_start_;
  const byte* section_end_;

  // Reads the section code/name at the current position and sets up
  // the embedder fields.
  void next() {
    if (!decoder_->more()) {
      section_code_ = kUnknownSectionCode;
      return;
    }
    section_start_ = decoder_->pc();
    uint8_t section_code = decoder_->consume_u8("section code");
    // Read and check the section size.
    uint32_t section_length = decoder_->consume_u32v("section length");

    payload_start_ = decoder_->pc();
    if (decoder_->checkAvailable(section_length)) {
      // Get the limit of the section within the module.
      section_end_ = payload_start_ + section_length;
    } else {
      // The section would extend beyond the end of the module.
      section_end_ = payload_start_;
    }

    if (section_code == kUnknownSectionCode) {
      // Check for the known "name", "sourceMappingURL", or "compilationHints"
      // section.
      // To identify the unknown section we set the end of the decoder bytes to
      // the end of the custom section, so that we do not read the section name
      // beyond the end of the section.
      const byte* module_end = decoder_->end();
      decoder_->set_end(section_end_);
      section_code = IdentifyUnknownSectionInternal(decoder_);
      if (decoder_->ok()) decoder_->set_end(module_end);
      // As a side effect, the above function will forward the decoder to after
      // the identifier string.
      payload_start_ = decoder_->pc();
    } else if (!IsValidSectionCode(section_code)) {
      decoder_->errorf(decoder_->pc(), "unknown section code #0x%02x",
                       section_code);
      section_code = kUnknownSectionCode;
    }
    section_code_ = decoder_->failed() ? kUnknownSectionCode
                                       : static_cast<SectionCode>(section_code);

    if (section_code_ == kUnknownSectionCode && section_end_ > decoder_->pc()) {
      // skip to the end of the unknown section.
      uint32_t remaining = static_cast<uint32_t>(section_end_ - decoder_->pc());
      decoder_->consume_bytes(remaining, "section payload");
    }
  }
};

}  // namespace

// The main logic for decoding the bytes of a module.
class ModuleDecoderImpl : public Decoder {
 public:
  explicit ModuleDecoderImpl(const WasmFeatures& enabled, ModuleOrigin origin)
      : Decoder(nullptr, nullptr),
        enabled_features_(enabled),
        origin_(origin) {}

  ModuleDecoderImpl(const WasmFeatures& enabled, const byte* module_start,
                    const byte* module_end, ModuleOrigin origin)
      : Decoder(module_start, module_end),
        enabled_features_(enabled),
        module_start_(module_start),
        module_end_(module_end),
        origin_(origin) {
    if (end_ < start_) {
      error(start_, "end is less than start");
      end_ = start_;
    }
  }

  void onFirstError() override {
    pc_ = end_;  // On error, terminate section decoding loop.
  }

  void DumpModule(const base::Vector<const byte> module_bytes) {
    std::string path;
    if (FLAG_dump_wasm_module_path) {
      path = FLAG_dump_wasm_module_path;
      if (path.size() &&
          !base::OS::isDirectorySeparator(path[path.size() - 1])) {
        path += base::OS::DirectorySeparator();
      }
    }
    // File are named `HASH.{ok,failed}.wasm`.
    size_t hash = base::hash_range(module_bytes.begin(), module_bytes.end());
    base::EmbeddedVector<char, 32> buf;
    SNPrintF(buf, "%016zx.%s.wasm", hash, ok() ? "ok" : "failed");
    path += buf.begin();
    size_t rv = 0;
    if (FILE* file = base::OS::FOpen(path.c_str(), "wb")) {
      rv = fwrite(module_bytes.begin(), module_bytes.length(), 1, file);
      base::Fclose(file);
    }
    if (rv != 1) {
      OFStream os(stderr);
      os << "Error while dumping wasm file to " << path << std::endl;
    }
  }

  void StartDecoding(Counters* counters, AccountingAllocator* allocator) {
    CHECK_NULL(module_);
    SetCounters(counters);
    module_.reset(
        new WasmModule(std::make_unique<Zone>(allocator, "signatures")));
    module_->initial_pages = 0;
    module_->maximum_pages = 0;
    module_->mem_export = false;
    module_->origin = origin_;
  }

  void DecodeModuleHeader(base::Vector<const uint8_t> bytes, uint8_t offset) {
    if (failed()) return;
    Reset(bytes, offset);

    const byte* pos = pc_;
    uint32_t magic_word = consume_u32("wasm magic");
#define BYTES(x) (x & 0xFF), (x >> 8) & 0xFF, (x >> 16) & 0xFF, (x >> 24) & 0xFF
    if (magic_word != kWasmMagic) {
      errorf(pos,
             "expected magic word %02x %02x %02x %02x, "
             "found %02x %02x %02x %02x",
             BYTES(kWasmMagic), BYTES(magic_word));
    }

    pos = pc_;
    {
      uint32_t magic_version = consume_u32("wasm version");
      if (magic_version != kWasmVersion) {
        errorf(pos,
               "expected version %02x %02x %02x %02x, "
               "found %02x %02x %02x %02x",
               BYTES(kWasmVersion), BYTES(magic_version));
      }
    }
#undef BYTES
  }

  bool CheckSectionOrder(SectionCode section_code,
                         SectionCode prev_section_code,
                         SectionCode next_section_code) {
    if (next_ordered_section_ > next_section_code) {
      errorf(pc(), "The %s section must appear before the %s section",
             SectionName(section_code), SectionName(next_section_code));
      return false;
    }
    if (next_ordered_section_ <= prev_section_code) {
      next_ordered_section_ = prev_section_code + 1;
    }
    return true;
  }

  bool CheckUnorderedSection(SectionCode section_code) {
    if (has_seen_unordered_section(section_code)) {
      errorf(pc(), "Multiple %s sections not allowed",
             SectionName(section_code));
      return false;
    }
    set_seen_unordered_section(section_code);
    return true;
  }

  void DecodeSection(SectionCode section_code,
                     base::Vector<const uint8_t> bytes, uint32_t offset,
                     bool verify_functions = true) {
    if (failed()) return;
    Reset(bytes, offset);
    TRACE("Section: %s\n", SectionName(section_code));
    TRACE("Decode Section %p - %p\n", bytes.begin(), bytes.end());

    // Check if the section is out-of-order.
    if (section_code < next_ordered_section_ &&
        section_code < kFirstUnorderedSection) {
      errorf(pc(), "unexpected section <%s>", SectionName(section_code));
      return;
    }

    switch (section_code) {
      case kUnknownSectionCode:
        break;
      case kDataCountSectionCode:
        if (!CheckUnorderedSection(section_code)) return;
        // If wasm-gc is enabled, we allow the data cound section anywhere in
        // the module.
        if (!enabled_features_.has_gc() &&
            !CheckSectionOrder(section_code, kElementSectionCode,
                               kCodeSectionCode)) {
          return;
        }
        break;
      case kTagSectionCode:
        if (!CheckUnorderedSection(section_code)) return;
        if (!CheckSectionOrder(section_code, kMemorySectionCode,
                               kGlobalSectionCode)) {
          return;
        }
        break;
      case kNameSectionCode:
        // TODO(titzer): report out of place name section as a warning.
        // Be lenient with placement of name section. All except first
        // occurrence are ignored.
      case kSourceMappingURLSectionCode:
        // sourceMappingURL is a custom section and currently can occur anywhere
        // in the module. In case of multiple sourceMappingURL sections, all
        // except the first occurrence are ignored.
      case kDebugInfoSectionCode:
        // .debug_info is a custom section containing core DWARF information
        // if produced by compiler. Its presence likely means that Wasm was
        // built in a debug mode.
      case kExternalDebugInfoSectionCode:
        // external_debug_info is a custom section containing a reference to an
        // external symbol file.
      case kCompilationHintsSectionCode:
        // TODO(frgossen): report out of place compilation hints section as a
        // warning.
        // Be lenient with placement of compilation hints section. All except
        // first occurrence after function section and before code section are
        // ignored.
        break;
      case kBranchHintsSectionCode:
        // TODO(yuri): report out of place branch hints section as a
        // warning.
        // Be lenient with placement of compilation hints section. All except
        // first occurrence after function section and before code section are
        // ignored.
        break;
      default:
        next_ordered_section_ = section_code + 1;
        break;
    }

    switch (section_code) {
      case kUnknownSectionCode:
        break;
      case kTypeSectionCode:
        DecodeTypeSection();
        break;
      case kImportSectionCode:
        DecodeImportSection();
        break;
      case kFunctionSectionCode:
        DecodeFunctionSection();
        break;
      case kTableSectionCode:
        DecodeTableSection();
        break;
      case kMemorySectionCode:
        DecodeMemorySection();
        break;
      case kGlobalSectionCode:
        DecodeGlobalSection();
        break;
      case kExportSectionCode:
        DecodeExportSection();
        break;
      case kStartSectionCode:
        DecodeStartSection();
        break;
      case kCodeSectionCode:
        DecodeCodeSection(verify_functions);
        break;
      case kElementSectionCode:
        DecodeElementSection();
        break;
      case kDataSectionCode:
        DecodeDataSection();
        break;
      case kNameSectionCode:
        DecodeNameSection();
        break;
      case kSourceMappingURLSectionCode:
        DecodeSourceMappingURLSection();
        break;
      case kDebugInfoSectionCode:
        // If there is an explicit source map, prefer it over DWARF info.
        if (module_->debug_symbols.type == WasmDebugSymbols::Type::None) {
          module_->debug_symbols = {WasmDebugSymbols::Type::EmbeddedDWARF, {}};
        }
        consume_bytes(static_cast<uint32_t>(end_ - start_), ".debug_info");
        break;
      case kExternalDebugInfoSectionCode:
        DecodeExternalDebugInfoSection();
        break;
      case kCompilationHintsSectionCode:
        if (enabled_features_.has_compilation_hints()) {
          DecodeCompilationHintsSection();
        } else {
          // Ignore this section when feature was disabled. It is an optional
          // custom section anyways.
          consume_bytes(static_cast<uint32_t>(end_ - start_), nullptr);
        }
        break;
      case kBranchHintsSectionCode:
        if (enabled_features_.has_branch_hinting()) {
          DecodeBranchHintsSection();
        } else {
          // Ignore this section when feature was disabled. It is an optional
          // custom section anyways.
          consume_bytes(static_cast<uint32_t>(end_ - start_), nullptr);
        }
        break;
      case kDataCountSectionCode:
        DecodeDataCountSection();
        break;
      case kTagSectionCode:
        if (enabled_features_.has_eh()) {
          DecodeTagSection();
        } else {
          errorf(pc(),
                 "unexpected section <%s> (enable with --experimental-wasm-eh)",
                 SectionName(section_code));
        }
        break;
      default:
        errorf(pc(), "unexpected section <%s>", SectionName(section_code));
        return;
    }

    if (pc() != bytes.end()) {
      const char* msg = pc() < bytes.end() ? "shorter" : "longer";
      errorf(pc(),
             "section was %s than expected size "
             "(%zu bytes expected, %zu decoded)",
             msg, bytes.size(), static_cast<size_t>(pc() - bytes.begin()));
    }
  }

  TypeDefinition consume_base_type_definition() {
    DCHECK(enabled_features_.has_gc());
    uint8_t kind = consume_u8("type kind");
    switch (kind) {
      case kWasmFunctionTypeCode: {
        const FunctionSig* sig = consume_sig(module_->signature_zone.get());
        return {sig, kNoSuperType};
      }
      case kWasmStructTypeCode: {
        const StructType* type = consume_struct(module_->signature_zone.get());
        return {type, kNoSuperType};
      }
      case kWasmArrayTypeCode: {
        const ArrayType* type = consume_array(module_->signature_zone.get());
        return {type, kNoSuperType};
      }
      case kWasmFunctionNominalCode:
      case kWasmArrayNominalCode:
      case kWasmStructNominalCode:
        errorf(pc() - 1,
               "mixing nominal and isorecursive types is not allowed");
        return {};
      default:
        errorf(pc() - 1, "unknown type form: %d", kind);
        return {};
    }
  }

  bool check_supertype(uint32_t supertype) {
    if (V8_UNLIKELY(supertype >= module_->types.size())) {
      errorf(pc(), "type %zu: forward-declared supertype %d",
             module_->types.size(), supertype);
      return false;
    }
    return true;
  }

  TypeDefinition consume_nominal_type_definition() {
    DCHECK(enabled_features_.has_gc());
    size_t num_types = module_->types.size();
    uint8_t kind = consume_u8("type kind");
    switch (kind) {
      case kWasmFunctionNominalCode: {
        const FunctionSig* sig = consume_sig(module_->signature_zone.get());
        uint32_t super_index = kNoSuperType;
        HeapType super_type = consume_super_type();
        if (super_type.is_index()) {
          super_index = super_type.representation();
        } else if (V8_UNLIKELY(super_type != HeapType::kFunc)) {
          errorf(pc() - 1, "type %zu: invalid supertype %d", num_types,
                 super_type.code());
          return {};
        }
        return {sig, super_index};
      }
      case kWasmStructNominalCode: {
        const StructType* type = consume_struct(module_->signature_zone.get());
        uint32_t super_index = kNoSuperType;
        HeapType super_type = consume_super_type();
        if (super_type.is_index()) {
          super_index = super_type.representation();
        } else if (V8_UNLIKELY(super_type != HeapType::kData)) {
          errorf(pc() - 1, "type %zu: invalid supertype %d", num_types,
                 super_type.code());
          return {};
        }
        return {type, super_index};
      }
      case kWasmArrayNominalCode: {
        const ArrayType* type = consume_array(module_->signature_zone.get());
        uint32_t super_index = kNoSuperType;
        HeapType super_type = consume_super_type();
        if (super_type.is_index()) {
          super_index = super_type.representation();
        } else if (V8_UNLIKELY(super_type != HeapType::kData)) {
          errorf(pc() - 1, "type %zu: invalid supertype %d", num_types,
                 super_type.code());
          return {};
        }
        return {type, super_index};
      }
      case kWasmFunctionTypeCode:
      case kWasmArrayTypeCode:
      case kWasmStructTypeCode:
      case kWasmSubtypeCode:
      case kWasmRecursiveTypeGroupCode:
        errorf(pc() - 1,
               "mixing nominal and isorecursive types is not allowed");
        return {};
      default:
        errorf(pc() - 1, "unknown type form: %d", kind);
        return {};
    }
  }

  TypeDefinition consume_subtype_definition() {
    DCHECK(enabled_features_.has_gc());
    uint8_t kind = read_u8<Decoder::kFullValidation>(pc(), "type kind");
    if (kind == kWasmSubtypeCode) {
      consume_bytes(1, "subtype definition");
      constexpr uint32_t kMaximumSupertypes = 1;
      uint32_t supertype_count =
          consume_count("supertype count", kMaximumSupertypes);
      uint32_t supertype =
          supertype_count == 1 ? consume_u32v("supertype") : kNoSuperType;
      if (!check_supertype(supertype)) return {};
      TypeDefinition type = consume_base_type_definition();
      type.supertype = supertype;
      return type;
    } else {
      return consume_base_type_definition();
    }
  }

  void DecodeTypeSection() {
    TypeCanonicalizer* type_canon = GetTypeCanonicalizer();
    uint32_t types_count = consume_count("types count", kV8MaxWasmTypes);

    // Non wasm-gc type section decoding.
    if (!enabled_features_.has_gc()) {
      module_->types.reserve(types_count);
      for (uint32_t i = 0; i < types_count; ++i) {
        TRACE("DecodeSignature[%d] module+%d\n", i,
              static_cast<int>(pc_ - start_));
        expect_u8("signature definition", kWasmFunctionTypeCode);
        const FunctionSig* sig = consume_sig(module_->signature_zone.get());
        if (!ok()) break;
        module_->add_signature(sig, kNoSuperType);
        if (FLAG_wasm_type_canonicalization) {
          type_canon->AddRecursiveGroup(module_.get(), 1);
        }
      }
      return;
    }

    if (types_count > 0) {
      uint8_t first_type_opcode = this->read_u8<Decoder::kFullValidation>(pc());
      if (first_type_opcode == kWasmFunctionNominalCode ||
          first_type_opcode == kWasmStructNominalCode ||
          first_type_opcode == kWasmArrayNominalCode) {
        // wasm-gc nominal type section decoding.
        // In a nominal module, all types belong in the same recursive group. We
        // use the type vector's capacity to mark the end of the current
        // recursive group.
        module_->types.reserve(types_count);
        for (uint32_t i = 0; ok() && i < types_count; ++i) {
          TRACE("DecodeType[%d] module+%d\n", i,
                static_cast<int>(pc_ - start_));
          TypeDefinition type = consume_nominal_type_definition();
          if (ok()) module_->add_type(type);
        }
        if (ok() && FLAG_wasm_type_canonicalization) {
          type_canon->AddRecursiveGroup(module_.get(), types_count);
        }
      } else {
        // wasm-gc isorecursive type section decoding.
        for (uint32_t i = 0; ok() && i < types_count; ++i) {
          TRACE("DecodeType[%d] module+%d\n", i,
                static_cast<int>(pc_ - start_));
          uint8_t kind = read_u8<Decoder::kFullValidation>(pc(), "type kind");
          if (kind == kWasmRecursiveTypeGroupCode) {
            consume_bytes(1, "rec. group definition");
            uint32_t group_size =
                consume_count("recursive group size", kV8MaxWasmTypes);
            if (module_->types.size() + group_size > kV8MaxWasmTypes) {
              errorf(pc(), "Type definition count exeeds maximum %zu",
                     kV8MaxWasmTypes);
              return;
            }
            // Reserve space for the current recursive group, so we are
            // allowed to reference its elements.
            module_->types.reserve(module_->types.size() + group_size);
            for (uint32_t i = 0; i < group_size; i++) {
              TypeDefinition type = consume_subtype_definition();
              if (ok()) module_->add_type(type);
            }
            if (ok() && FLAG_wasm_type_canonicalization) {
              type_canon->AddRecursiveGroup(module_.get(), group_size);
            }
          } else {
            TypeDefinition type = consume_subtype_definition();
            if (ok()) {
              module_->add_type(type);
              if (FLAG_wasm_type_canonicalization) {
                type_canon->AddRecursiveGroup(module_.get(), 1);
              }
            }
          }
        }
      }
    }

    // Check validity of explicitly defined supertypes.
    const WasmModule* module = module_.get();
    for (uint32_t i = 0; ok() && i < types_count; ++i) {
      uint32_t explicit_super = module_->supertype(i);
      if (explicit_super == kNoSuperType) continue;
      DCHECK_LT(explicit_super, types_count);  // {consume_super_type} checks.
      int depth = GetSubtypingDepth(module, i);
      if (depth > static_cast<int>(kV8MaxRttSubtypingDepth)) {
        errorf("type %d: subtyping depth is greater than allowed", i);
        continue;
      }
      // TODO(7748): Replace this with a DCHECK once we reject inheritance
      // cycles for nominal modules.
      if (depth == -1) {
        errorf("type %d: cyclic inheritance", i);
        continue;
      }
      if (!ValidSubtypeDefinition(i, explicit_super, module, module)) {
        errorf("type %d has invalid explicit supertype %d", i, explicit_super);
        continue;
      }
    }
    module_->signature_map.Freeze();
  }

  void DecodeImportSection() {
    uint32_t import_table_count =
        consume_count("imports count", kV8MaxWasmImports);
    module_->import_table.reserve(import_table_count);
    for (uint32_t i = 0; ok() && i < import_table_count; ++i) {
      TRACE("DecodeImportTable[%d] module+%d\n", i,
            static_cast<int>(pc_ - start_));

      module_->import_table.push_back({
          {0, 0},             // module_name
          {0, 0},             // field_name
          kExternalFunction,  // kind
          0                   // index
      });
      WasmImport* import = &module_->import_table.back();
      const byte* pos = pc_;
      import->module_name = consume_string(this, true, "module name");
      import->field_name = consume_string(this, true, "field name");
      import->kind =
          static_cast<ImportExportKindCode>(consume_u8("import kind"));
      switch (import->kind) {
        case kExternalFunction: {
          // ===== Imported function ===========================================
          import->index = static_cast<uint32_t>(module_->functions.size());
          module_->num_imported_functions++;
          module_->functions.push_back({nullptr,        // sig
                                        import->index,  // func_index
                                        0,              // sig_index
                                        {0, 0},         // code
                                        0,              // feedback slots
                                        true,           // imported
                                        false,          // exported
                                        false});        // declared
          WasmFunction* function = &module_->functions.back();
          function->sig_index =
              consume_sig_index(module_.get(), &function->sig);
          break;
        }
        case kExternalTable: {
          // ===== Imported table ==============================================
          import->index = static_cast<uint32_t>(module_->tables.size());
          module_->num_imported_tables++;
          module_->tables.emplace_back();
          WasmTable* table = &module_->tables.back();
          table->imported = true;
          const byte* type_position = pc();
          ValueType type = consume_reference_type();
          if (!WasmTable::IsValidTableType(type, module_.get())) {
            errorf(type_position, "Invalid table type %s", type.name().c_str());
            break;
          }
          table->type = type;
          uint8_t flags = validate_table_flags("element count");
          consume_resizable_limits(
              "element count", "elements", std::numeric_limits<uint32_t>::max(),
              &table->initial_size, &table->has_maximum_size,
              std::numeric_limits<uint32_t>::max(), &table->maximum_size,
              flags);
          break;
        }
        case kExternalMemory: {
          // ===== Imported memory =============================================
          if (!AddMemory(module_.get())) break;
          uint8_t flags = validate_memory_flags(&module_->has_shared_memory,
                                                &module_->is_memory64);
          consume_resizable_limits(
              "memory", "pages", kSpecMaxMemoryPages, &module_->initial_pages,
              &module_->has_maximum_pages, kSpecMaxMemoryPages,
              &module_->maximum_pages, flags);
          break;
        }
        case kExternalGlobal: {
          // ===== Imported global =============================================
          import->index = static_cast<uint32_t>(module_->globals.size());
          module_->globals.push_back({kWasmVoid, false, {}, {0}, true, false});
          WasmGlobal* global = &module_->globals.back();
          global->type = consume_value_type();
          global->mutability = consume_mutability();
          if (global->mutability) {
            module_->num_imported_mutable_globals++;
          }
          break;
        }
        case kExternalTag: {
          // ===== Imported tag ================================================
          if (!enabled_features_.has_eh()) {
            errorf(pos, "unknown import kind 0x%02x", import->kind);
            break;
          }
          import->index = static_cast<uint32_t>(module_->tags.size());
          const WasmTagSig* tag_sig = nullptr;
          consume_exception_attribute();  // Attribute ignored for now.
          consume_tag_sig_index(module_.get(), &tag_sig);
          module_->tags.emplace_back(tag_sig);
          break;
        }
        default:
          errorf(pos, "unknown import kind 0x%02x", import->kind);
          break;
      }
    }
  }

  void DecodeFunctionSection() {
    uint32_t functions_count =
        consume_count("functions count", kV8MaxWasmFunctions);
    auto counter =
        SELECT_WASM_COUNTER(GetCounters(), origin_, wasm_functions_per, module);
    counter->AddSample(static_cast<int>(functions_count));
    DCHECK_EQ(module_->functions.size(), module_->num_imported_functions);
    uint32_t total_function_count =
        module_->num_imported_functions + functions_count;
    module_->functions.reserve(total_function_count);
    module_->num_declared_functions = functions_count;
    for (uint32_t i = 0; i < functions_count; ++i) {
      uint32_t func_index = static_cast<uint32_t>(module_->functions.size());
      module_->functions.push_back({nullptr,     // sig
                                    func_index,  // func_index
                                    0,           // sig_index
                                    {0, 0},      // code
                                    0,           // feedback slots
                                    false,       // imported
                                    false,       // exported
                                    false});     // declared
      WasmFunction* function = &module_->functions.back();
      function->sig_index = consume_sig_index(module_.get(), &function->sig);
      if (!ok()) return;
    }
    DCHECK_EQ(module_->functions.size(), total_function_count);
  }

  void DecodeTableSection() {
    uint32_t table_count = consume_count("table count", kV8MaxWasmTables);

    for (uint32_t i = 0; ok() && i < table_count; i++) {
      module_->tables.emplace_back();
      WasmTable* table = &module_->tables.back();
      const byte* type_position = pc();
      ValueType table_type = consume_reference_type();
      if (!WasmTable::IsValidTableType(table_type, module_.get())) {
        error(type_position,
              "Currently, only externref and function references are allowed "
              "as table types");
        continue;
      }
      table->type = table_type;
      uint8_t flags = validate_table_flags("table elements");
      consume_resizable_limits(
          "table elements", "elements", std::numeric_limits<uint32_t>::max(),
          &table->initial_size, &table->has_maximum_size,
          std::numeric_limits<uint32_t>::max(), &table->maximum_size, flags);
      if (!table_type.is_defaultable()) {
        table->initial_value = consume_init_expr(module_.get(), table_type);
      }
    }
  }

  void DecodeMemorySection() {
    uint32_t memory_count = consume_count("memory count", kV8MaxWasmMemories);

    for (uint32_t i = 0; ok() && i < memory_count; i++) {
      if (!AddMemory(module_.get())) break;
      uint8_t flags = validate_memory_flags(&module_->has_shared_memory,
                                            &module_->is_memory64);
      consume_resizable_limits("memory", "pages", kSpecMaxMemoryPages,
                               &module_->initial_pages,
                               &module_->has_maximum_pages, kSpecMaxMemoryPages,
                               &module_->maximum_pages, flags);
    }
  }

  void DecodeGlobalSection() {
    uint32_t globals_count = consume_count("globals count", kV8MaxWasmGlobals);
    uint32_t imported_globals = static_cast<uint32_t>(module_->globals.size());
    // It is important to not resize the globals vector from the beginning,
    // because we use its current size when decoding the initializer.
    module_->globals.reserve(imported_globals + globals_count);
    for (uint32_t i = 0; ok() && i < globals_count; ++i) {
      TRACE("DecodeGlobal[%d] module+%d\n", i, static_cast<int>(pc_ - start_));
      ValueType type = consume_value_type();
      bool mutability = consume_mutability();
      if (failed()) break;
      ConstantExpression init = consume_init_expr(module_.get(), type);
      module_->globals.push_back({type, mutability, init, {0}, false, false});
    }
    if (ok()) CalculateGlobalOffsets(module_.get());
  }

  void DecodeExportSection() {
    uint32_t export_table_count =
        consume_count("exports count", kV8MaxWasmExports);
    module_->export_table.reserve(export_table_count);
    for (uint32_t i = 0; ok() && i < export_table_count; ++i) {
      TRACE("DecodeExportTable[%d] module+%d\n", i,
            static_cast<int>(pc_ - start_));

      module_->export_table.push_back({
          {0, 0},             // name
          kExternalFunction,  // kind
          0                   // index
      });
      WasmExport* exp = &module_->export_table.back();

      exp->name = consume_string(this, true, "field name");

      const byte* pos = pc();
      exp->kind = static_cast<ImportExportKindCode>(consume_u8("export kind"));
      switch (exp->kind) {
        case kExternalFunction: {
          WasmFunction* func = nullptr;
          exp->index =
              consume_func_index(module_.get(), &func, "export function index");

          if (failed()) break;
          DCHECK_NOT_NULL(func);

          module_->num_exported_functions++;
          func->exported = true;
          // Exported functions are considered "declared".
          func->declared = true;
          break;
        }
        case kExternalTable: {
          WasmTable* table = nullptr;
          exp->index = consume_table_index(module_.get(), &table);
          if (table) table->exported = true;
          break;
        }
        case kExternalMemory: {
          uint32_t index = consume_u32v("memory index");
          // TODO(titzer): This should become more regular
          // once we support multiple memories.
          if (!module_->has_memory || index != 0) {
            error("invalid memory index != 0");
          }
          module_->mem_export = true;
          break;
        }
        case kExternalGlobal: {
          WasmGlobal* global = nullptr;
          exp->index = consume_global_index(module_.get(), &global);
          if (global) {
            global->exported = true;
          }
          break;
        }
        case kExternalTag: {
          if (!enabled_features_.has_eh()) {
            errorf(pos, "invalid export kind 0x%02x", exp->kind);
            break;
          }
          WasmTag* tag = nullptr;
          exp->index = consume_tag_index(module_.get(), &tag);
          break;
        }
        default:
          errorf(pos, "invalid export kind 0x%02x", exp->kind);
          break;
      }
    }
    // Check for duplicate exports (except for asm.js).
    if (ok() && origin_ == kWasmOrigin && module_->export_table.size() > 1) {
      std::vector<WasmExport> sorted_exports(module_->export_table);

      auto cmp_less = [this](const WasmExport& a, const WasmExport& b) {
        // Return true if a < b.
        if (a.name.length() != b.name.length()) {
          return a.name.length() < b.name.length();
        }
        const byte* left = start() + GetBufferRelativeOffset(a.name.offset());
        const byte* right = start() + GetBufferRelativeOffset(b.name.offset());
        return memcmp(left, right, a.name.length()) < 0;
      };
      std::stable_sort(sorted_exports.begin(), sorted_exports.end(), cmp_less);

      auto it = sorted_exports.begin();
      WasmExport* last = &*it++;
      for (auto end = sorted_exports.end(); it != end; last = &*it++) {
        DCHECK(!cmp_less(*it, *last));  // Vector must be sorted.
        if (!cmp_less(*last, *it)) {
          const byte* pc = start() + GetBufferRelativeOffset(it->name.offset());
          TruncatedUserString<> name(pc, it->name.length());
          errorf(pc, "Duplicate export name '%.*s' for %s %d and %s %d",
                 name.length(), name.start(), ExternalKindName(last->kind),
                 last->index, ExternalKindName(it->kind), it->index);
          break;
        }
      }
    }
  }

  void DecodeStartSection() {
    WasmFunction* func;
    const byte* pos = pc_;
    module_->start_function_index =
        consume_func_index(module_.get(), &func, "start function index");
    if (func &&
        (func->sig->parameter_count() > 0 || func->sig->return_count() > 0)) {
      error(pos, "invalid start function: non-zero parameter or return count");
    }
  }

  void DecodeElementSection() {
    uint32_t element_count =
        consume_count("element count", FLAG_wasm_max_table_size);

    for (uint32_t i = 0; i < element_count; ++i) {
      WasmElemSegment segment = consume_element_segment_header();
      if (failed()) return;
      DCHECK_NE(segment.type, kWasmBottom);

      uint32_t num_elem =
          consume_count("number of elements", max_table_init_entries());

      for (uint32_t j = 0; j < num_elem; j++) {
        ConstantExpression entry =
            segment.element_type == WasmElemSegment::kExpressionElements
                ? consume_init_expr(module_.get(), segment.type)
                : ConstantExpression::RefFunc(
                      consume_element_func_index(segment.type));
        if (failed()) return;
        segment.entries.push_back(entry);
      }
      module_->elem_segments.push_back(std::move(segment));
    }
  }

  void DecodeCodeSection(bool verify_functions) {
    StartCodeSection();
    uint32_t code_section_start = pc_offset();
    uint32_t functions_count = consume_u32v("functions count");
    CheckFunctionsCount(functions_count, code_section_start);
    for (uint32_t i = 0; ok() && i < functions_count; ++i) {
      const byte* pos = pc();
      uint32_t size = consume_u32v("body size");
      if (size > kV8MaxWasmFunctionSize) {
        errorf(pos, "size %u > maximum function size %zu", size,
               kV8MaxWasmFunctionSize);
        return;
      }
      uint32_t offset = pc_offset();
      consume_bytes(size, "function body");
      if (failed()) break;
      DecodeFunctionBody(i, size, offset, verify_functions);
    }
    DCHECK_GE(pc_offset(), code_section_start);
    set_code_section(code_section_start, pc_offset() - code_section_start);
  }

  void StartCodeSection() {
    if (ok()) {
      // Make sure global offset were calculated before they get accessed during
      // function compilation.
      CalculateGlobalOffsets(module_.get());
    }
  }

  bool CheckFunctionsCount(uint32_t functions_count, uint32_t error_offset) {
    if (functions_count != module_->num_declared_functions) {
      errorf(error_offset, "function body count %u mismatch (%u expected)",
             functions_count, module_->num_declared_functions);
      return false;
    }
    return true;
  }

  void DecodeFunctionBody(uint32_t index, uint32_t length, uint32_t offset,
                          bool verify_functions) {
    WasmFunction* function =
        &module_->functions[index + module_->num_imported_functions];
    function->code = {offset, length};
    if (verify_functions) {
      ModuleWireBytes bytes(module_start_, module_end_);
      VerifyFunctionBody(module_->signature_zone->allocator(),
                         index + module_->num_imported_functions, bytes,
                         module_.get(), function);
    }
  }

  bool CheckDataSegmentsCount(uint32_t data_segments_count) {
    if (has_seen_unordered_section(kDataCountSectionCode) &&
        data_segments_count != module_->num_declared_data_segments) {
      errorf(pc(), "data segments count %u mismatch (%u expected)",
             data_segments_count, module_->num_declared_data_segments);
      return false;
    }
    return true;
  }

  void DecodeDataSection() {
    uint32_t data_segments_count =
        consume_count("data segments count", kV8MaxWasmDataSegments);
    if (!CheckDataSegmentsCount(data_segments_count)) return;

    module_->data_segments.reserve(data_segments_count);
    for (uint32_t i = 0; ok() && i < data_segments_count; ++i) {
      const byte* pos = pc();
      TRACE("DecodeDataSegment[%d] module+%d\n", i,
            static_cast<int>(pc_ - start_));

      bool is_active;
      uint32_t memory_index;
      ConstantExpression dest_addr;
      consume_data_segment_header(&is_active, &memory_index, &dest_addr);
      if (failed()) break;

      if (is_active) {
        if (!module_->has_memory) {
          error("cannot load data without memory");
          break;
        }
        if (memory_index != 0) {
          errorf(pos, "illegal memory index %u != 0", memory_index);
          break;
        }
      }

      uint32_t source_length = consume_u32v("source size");
      uint32_t source_offset = pc_offset();

      if (is_active) {
        module_->data_segments.emplace_back(std::move(dest_addr));
      } else {
        module_->data_segments.emplace_back();
      }

      WasmDataSegment* segment = &module_->data_segments.back();

      consume_bytes(source_length, "segment data");
      if (failed()) break;

      segment->source = {source_offset, source_length};
    }
  }

  void DecodeNameSection() {
    // TODO(titzer): find a way to report name errors as warnings.
    // Ignore all but the first occurrence of name section.
    if (!has_seen_unordered_section(kNameSectionCode)) {
      set_seen_unordered_section(kNameSectionCode);
      // Use an inner decoder so that errors don't fail the outer decoder.
      Decoder inner(start_, pc_, end_, buffer_offset_);
      // Decode all name subsections.
      // Be lenient with their order.
      while (inner.ok() && inner.more()) {
        uint8_t name_type = inner.consume_u8("name type");
        if (name_type & 0x80) inner.error("name type if not varuint7");

        uint32_t name_payload_len = inner.consume_u32v("name payload length");
        if (!inner.checkAvailable(name_payload_len)) break;

        // Decode module name, ignore the rest.
        // Function and local names will be decoded when needed.
        if (name_type == NameSectionKindCode::kModuleCode) {
          WireBytesRef name = consume_string(&inner, false, "module name");
          if (inner.ok() && validate_utf8(&inner, name)) {
            module_->name = name;
          }
        } else {
          inner.consume_bytes(name_payload_len, "name subsection payload");
        }
      }
    }
    // Skip the whole names section in the outer decoder.
    consume_bytes(static_cast<uint32_t>(end_ - start_), nullptr);
  }

  void DecodeSourceMappingURLSection() {
    Decoder inner(start_, pc_, end_, buffer_offset_);
    WireBytesRef url = wasm::consume_string(&inner, true, "module name");
    if (inner.ok() &&
        module_->debug_symbols.type != WasmDebugSymbols::Type::SourceMap) {
      module_->debug_symbols = {WasmDebugSymbols::Type::SourceMap, url};
    }
    set_seen_unordered_section(kSourceMappingURLSectionCode);
    consume_bytes(static_cast<uint32_t>(end_ - start_), nullptr);
  }

  void DecodeExternalDebugInfoSection() {
    Decoder inner(start_, pc_, end_, buffer_offset_);
    WireBytesRef url =
        wasm::consume_string(&inner, true, "external symbol file");
    // If there is an explicit source map, prefer it over DWARF info.
    if (inner.ok() &&
        module_->debug_symbols.type != WasmDebugSymbols::Type::SourceMap) {
      module_->debug_symbols = {WasmDebugSymbols::Type::ExternalDWARF, url};
      set_seen_unordered_section(kExternalDebugInfoSectionCode);
    }
    consume_bytes(static_cast<uint32_t>(end_ - start_), nullptr);
  }

  void DecodeCompilationHintsSection() {
    TRACE("DecodeCompilationHints module+%d\n", static_cast<int>(pc_ - start_));

    // TODO(frgossen): Find a way to report compilation hint errors as warnings.
    // All except first occurrence after function section and before code
    // section are ignored.
    const bool before_function_section =
        next_ordered_section_ <= kFunctionSectionCode;
    const bool after_code_section = next_ordered_section_ > kCodeSectionCode;
    if (before_function_section || after_code_section ||
        has_seen_unordered_section(kCompilationHintsSectionCode)) {
      return;
    }
    set_seen_unordered_section(kCompilationHintsSectionCode);

    // TODO(frgossen) Propagate errors to outer decoder in experimental phase.
    // We should use an inner decoder later and propagate its errors as
    // warnings.
    Decoder& decoder = *this;
    // Decoder decoder(start_, pc_, end_, buffer_offset_);

    // Ensure exactly one compilation hint per function.
    uint32_t hint_count = decoder.consume_u32v("compilation hint count");
    if (hint_count != module_->num_declared_functions) {
      decoder.errorf(decoder.pc(), "Expected %u compilation hints (%u found)",
                     module_->num_declared_functions, hint_count);
    }

    // Decode sequence of compilation hints.
    if (decoder.ok()) {
      module_->compilation_hints.reserve(hint_count);
    }
    for (uint32_t i = 0; decoder.ok() && i < hint_count; i++) {
      TRACE("DecodeCompilationHints[%d] module+%d\n", i,
            static_cast<int>(pc_ - start_));

      // Compilation hints are encoded in one byte each.
      // +-------+----------+---------------+----------+
      // | 2 bit | 2 bit    | 2 bit         | 2 bit    |
      // | ...   | Top tier | Baseline tier | Strategy |
      // +-------+----------+---------------+----------+
      uint8_t hint_byte = decoder.consume_u8("compilation hint");
      if (!decoder.ok()) break;

      // Validate the hint_byte.
      // For the compilation strategy, all 2-bit values are valid. For the tier,
      // only 0x0, 0x1, and 0x2 are allowed.
      static_assert(
          static_cast<int>(WasmCompilationHintTier::kDefault) == 0 &&
              static_cast<int>(WasmCompilationHintTier::kBaseline) == 1 &&
              static_cast<int>(WasmCompilationHintTier::kOptimized) == 2,
          "The check below assumes that 0x03 is the only invalid 2-bit number "
          "for a compilation tier");
      if (((hint_byte >> 2) & 0x03) == 0x03 ||
          ((hint_byte >> 4) & 0x03) == 0x03) {
        decoder.errorf(decoder.pc(),
                       "Invalid compilation hint %#04x (invalid tier 0x03)",
                       hint_byte);
        break;
      }

      // Decode compilation hint.
      WasmCompilationHint hint;
      hint.strategy =
          static_cast<WasmCompilationHintStrategy>(hint_byte & 0x03);
      hint.baseline_tier =
          static_cast<WasmCompilationHintTier>((hint_byte >> 2) & 0x03);
      hint.top_tier =
          static_cast<WasmCompilationHintTier>((hint_byte >> 4) & 0x03);

      // Ensure that the top tier never downgrades a compilation result. If
      // baseline and top tier are the same compilation will be invoked only
      // once.
      if (hint.top_tier < hint.baseline_tier &&
          hint.top_tier != WasmCompilationHintTier::kDefault) {
        decoder.errorf(decoder.pc(),
                       "Invalid compilation hint %#04x (forbidden downgrade)",
                       hint_byte);
      }

      // Happily accept compilation hint.
      if (decoder.ok()) {
        module_->compilation_hints.push_back(std::move(hint));
      }
    }

    // If section was invalid reset compilation hints.
    if (decoder.failed()) {
      module_->compilation_hints.clear();
    }

    // @TODO(frgossen) Skip the whole compilation hints section in the outer
    // decoder if inner decoder was used.
    // consume_bytes(static_cast<uint32_t>(end_ - start_), nullptr);
  }

  void DecodeBranchHintsSection() {
    TRACE("DecodeBranchHints module+%d\n", static_cast<int>(pc_ - start_));
    if (!has_seen_unordered_section(kBranchHintsSectionCode)) {
      set_seen_unordered_section(kBranchHintsSectionCode);
      // Use an inner decoder so that errors don't fail the outer decoder.
      Decoder inner(start_, pc_, end_, buffer_offset_);
      BranchHintInfo branch_hints;

      uint32_t func_count = inner.consume_u32v("number of functions");
      // Keep track of the previous function index to validate the ordering
      int64_t last_func_idx = -1;
      for (uint32_t i = 0; i < func_count; i++) {
        uint32_t func_idx = inner.consume_u32v("function index");
        if (int64_t(func_idx) <= last_func_idx) {
          inner.errorf("Invalid function index: %d", func_idx);
          break;
        }
        last_func_idx = func_idx;
        uint32_t num_hints = inner.consume_u32v("number of hints");
        BranchHintMap func_branch_hints;
        TRACE("DecodeBranchHints[%d] module+%d\n", func_idx,
              static_cast<int>(inner.pc() - inner.start()));
        // Keep track of the previous branch offset to validate the ordering
        int64_t last_br_off = -1;
        for (uint32_t j = 0; j < num_hints; ++j) {
          uint32_t br_off = inner.consume_u32v("branch instruction offset");
          if (int64_t(br_off) <= last_br_off) {
            inner.errorf("Invalid branch offset: %d", br_off);
            break;
          }
          last_br_off = br_off;
          uint32_t data_size = inner.consume_u32v("data size");
          if (data_size != 1) {
            inner.errorf("Invalid data size: %#x. Expected 1.", data_size);
            break;
          }
          uint32_t br_dir = inner.consume_u8("branch direction");
          TRACE("DecodeBranchHints[%d][%d] module+%d\n", func_idx, br_off,
                static_cast<int>(inner.pc() - inner.start()));
          WasmBranchHint hint;
          switch (br_dir) {
            case 0:
              hint = WasmBranchHint::kUnlikely;
              break;
            case 1:
              hint = WasmBranchHint::kLikely;
              break;
            default:
              hint = WasmBranchHint::kNoHint;
              inner.errorf(inner.pc(), "Invalid branch hint %#x", br_dir);
              break;
          }
          if (!inner.ok()) {
            break;
          }
          func_branch_hints.insert(br_off, hint);
        }
        if (!inner.ok()) {
          break;
        }
        branch_hints.emplace(func_idx, std::move(func_branch_hints));
      }
      // Extra unexpected bytes are an error.
      if (inner.more()) {
        inner.errorf("Unexpected extra bytes: %d\n",
                     static_cast<int>(inner.pc() - inner.start()));
      }
      // If everything went well, accept the hints for the module.
      if (inner.ok()) {
        module_->branch_hints = std::move(branch_hints);
      }
    }
    // Skip the whole branch hints section in the outer decoder.
    consume_bytes(static_cast<uint32_t>(end_ - start_), nullptr);
  }

  void DecodeDataCountSection() {
    module_->num_declared_data_segments =
        consume_count("data segments count", kV8MaxWasmDataSegments);
  }

  void DecodeTagSection() {
    uint32_t tag_count = consume_count("tag count", kV8MaxWasmTags);
    for (uint32_t i = 0; ok() && i < tag_count; ++i) {
      TRACE("DecodeTag[%d] module+%d\n", i, static_cast<int>(pc_ - start_));
      const WasmTagSig* tag_sig = nullptr;
      consume_exception_attribute();  // Attribute ignored for now.
      consume_tag_sig_index(module_.get(), &tag_sig);
      module_->tags.emplace_back(tag_sig);
    }
  }

  bool CheckMismatchedCounts() {
    // The declared vs. defined function count is normally checked when
    // decoding the code section, but we have to check it here too in case the
    // code section is absent.
    if (module_->num_declared_functions != 0) {
      DCHECK_LT(module_->num_imported_functions, module_->functions.size());
      // We know that the code section has been decoded if the first
      // non-imported function has its code set.
      if (!module_->functions[module_->num_imported_functions].code.is_set()) {
        errorf(pc(), "function count is %u, but code section is absent",
               module_->num_declared_functions);
        return false;
      }
    }
    // Perform a similar check for the DataCount and Data sections, where data
    // segments are declared but the Data section is absent.
    if (!CheckDataSegmentsCount(
            static_cast<uint32_t>(module_->data_segments.size()))) {
      return false;
    }
    return true;
  }

  ModuleResult FinishDecoding(bool verify_functions = true) {
    if (ok() && CheckMismatchedCounts()) {
      // We calculate the global offsets here, because there may not be a
      // global section and code section that would have triggered the
      // calculation before. Even without the globals section the calculation
      // is needed because globals can also be defined in the import section.
      CalculateGlobalOffsets(module_.get());
    }

    ModuleResult result = toResult(std::move(module_));
    if (verify_functions && result.ok() && intermediate_error_.has_error()) {
      // Copy error message and location.
      return ModuleResult{std::move(intermediate_error_)};
    }
    return result;
  }

  void set_code_section(uint32_t offset, uint32_t size) {
    module_->code = {offset, size};
  }

  // Decodes an entire module.
  ModuleResult DecodeModule(Counters* counters, AccountingAllocator* allocator,
                            bool verify_functions = true) {
    StartDecoding(counters, allocator);
    uint32_t offset = 0;
    base::Vector<const byte> orig_bytes(start(), end() - start());
    DecodeModuleHeader(base::VectorOf(start(), end() - start()), offset);
    if (failed()) {
      return FinishDecoding(verify_functions);
    }
    // Size of the module header.
    offset += 8;
    Decoder decoder(start_ + offset, end_, offset);

    WasmSectionIterator section_iter(&decoder);

    while (ok()) {
      // Shift the offset by the section header length
      offset += section_iter.payload_start() - section_iter.section_start();
      if (section_iter.section_code() != SectionCode::kUnknownSectionCode) {
        DecodeSection(section_iter.section_code(), section_iter.payload(),
                      offset, verify_functions);
      }
      // Shift the offset by the remaining section payload
      offset += section_iter.payload_length();
      if (!section_iter.more()) break;
      section_iter.advance(true);
    }

    if (FLAG_dump_wasm_module) DumpModule(orig_bytes);

    if (decoder.failed()) {
      return decoder.toResult<std::unique_ptr<WasmModule>>(nullptr);
    }

    return FinishDecoding(verify_functions);
  }

  // Decodes a single anonymous function starting at {start_}.
  FunctionResult DecodeSingleFunction(Zone* zone,
                                      const ModuleWireBytes& wire_bytes,
                                      const WasmModule* module,
                                      std::unique_ptr<WasmFunction> function) {
    pc_ = start_;
    expect_u8("type form", kWasmFunctionTypeCode);
    if (!ok()) return FunctionResult{std::move(intermediate_error_)};
    function->sig = consume_sig(zone);
    function->code = {off(pc_), static_cast<uint32_t>(end_ - pc_)};

    if (ok())
      VerifyFunctionBody(zone->allocator(), 0, wire_bytes, module,
                         function.get());

    if (intermediate_error_.has_error()) {
      return FunctionResult{std::move(intermediate_error_)};
    }

    return FunctionResult(std::move(function));
  }

  // Decodes a single function signature at {start}.
  const FunctionSig* DecodeFunctionSignature(Zone* zone, const byte* start) {
    pc_ = start;
    if (!expect_u8("type form", kWasmFunctionTypeCode)) return nullptr;
    const FunctionSig* result = consume_sig(zone);
    return ok() ? result : nullptr;
  }

  ConstantExpression DecodeInitExprForTesting(ValueType expected) {
    return consume_init_expr(module_.get(), expected);
  }

  const std::shared_ptr<WasmModule>& shared_module() const { return module_; }

  Counters* GetCounters() const {
    DCHECK_NOT_NULL(counters_);
    return counters_;
  }

  void SetCounters(Counters* counters) {
    DCHECK_NULL(counters_);
    counters_ = counters;
  }

 private:
  const WasmFeatures enabled_features_;
  std::shared_ptr<WasmModule> module_;
  const byte* module_start_ = nullptr;
  const byte* module_end_ = nullptr;
  Counters* counters_ = nullptr;
  // The type section is the first section in a module.
  uint8_t next_ordered_section_ = kFirstSectionInModule;
  // We store next_ordered_section_ as uint8_t instead of SectionCode so that
  // we can increment it. This static_assert should make sure that SectionCode
  // does not get bigger than uint8_t accidentially.
  static_assert(sizeof(ModuleDecoderImpl::next_ordered_section_) ==
                    sizeof(SectionCode),
                "type mismatch");
  uint32_t seen_unordered_sections_ = 0;
  static_assert(kBitsPerByte *
                        sizeof(ModuleDecoderImpl::seen_unordered_sections_) >
                    kLastKnownModuleSection,
                "not enough bits");
  WasmError intermediate_error_;
  ModuleOrigin origin_;
  AccountingAllocator allocator_;
  Zone init_expr_zone_{&allocator_, "initializer expression zone"};

  bool has_seen_unordered_section(SectionCode section_code) {
    return seen_unordered_sections_ & (1 << section_code);
  }

  void set_seen_unordered_section(SectionCode section_code) {
    seen_unordered_sections_ |= 1 << section_code;
  }

  uint32_t off(const byte* ptr) {
    return static_cast<uint32_t>(ptr - start_) + buffer_offset_;
  }

  bool AddMemory(WasmModule* module) {
    if (module->has_memory) {
      error("At most one memory is supported");
      return false;
    } else {
      module->has_memory = true;
      return true;
    }
  }

  // Calculate individual global offsets and total size of globals table. This
  // function should be called after all globals have been defined, which is
  // after the import section and the global section, but before the global
  // offsets are accessed, e.g. by the function compilers. The moment when this
  // function should be called is not well-defined, as the global section may
  // not exist. Therefore this function is called multiple times.
  void CalculateGlobalOffsets(WasmModule* module) {
    if (module->globals.empty() || module->untagged_globals_buffer_size != 0 ||
        module->tagged_globals_buffer_size != 0) {
      // This function has already been executed before, so we don't have to
      // execute it again.
      return;
    }
    uint32_t untagged_offset = 0;
    uint32_t tagged_offset = 0;
    uint32_t num_imported_mutable_globals = 0;
    for (WasmGlobal& global : module->globals) {
      if (global.mutability && global.imported) {
        global.index = num_imported_mutable_globals++;
      } else if (global.type.is_reference()) {
        global.offset = tagged_offset;
        // All entries in the tagged_globals_buffer have size 1.
        tagged_offset++;
      } else {
        int size = global.type.value_kind_size();
        untagged_offset = (untagged_offset + size - 1) & ~(size - 1);  // align
        global.offset = untagged_offset;
        untagged_offset += size;
      }
    }
    module->untagged_globals_buffer_size = untagged_offset;
    module->tagged_globals_buffer_size = tagged_offset;
  }

  // Verifies the body (code) of a given function.
  void VerifyFunctionBody(AccountingAllocator* allocator, uint32_t func_num,
                          const ModuleWireBytes& wire_bytes,
                          const WasmModule* module, WasmFunction* function) {
    WasmFunctionName func_name(function,
                               wire_bytes.GetNameOrNull(function, module));
    if (FLAG_trace_wasm_decoder) {
      StdoutStream{} << "Verifying wasm function " << func_name << std::endl;
    }
    FunctionBody body = {
        function->sig, function->code.offset(),
        start_ + GetBufferRelativeOffset(function->code.offset()),
        start_ + GetBufferRelativeOffset(function->code.end_offset())};

    WasmFeatures unused_detected_features = WasmFeatures::None();
    DecodeResult result = VerifyWasmCode(allocator, enabled_features_, module,
                                         &unused_detected_features, body);

    // If the decode failed and this is the first error, set error code and
    // location.
    if (result.failed() && intermediate_error_.empty()) {
      // Wrap the error message from the function decoder.
      std::ostringstream error_msg;
      error_msg << "in function " << func_name << ": "
                << result.error().message();
      intermediate_error_ = WasmError{result.error().offset(), error_msg.str()};
    }
  }

  uint32_t consume_sig_index(WasmModule* module, const FunctionSig** sig) {
    const byte* pos = pc_;
    uint32_t sig_index = consume_u32v("signature index");
    if (!module->has_signature(sig_index)) {
      errorf(pos, "signature index %u out of bounds (%d signatures)", sig_index,
             static_cast<int>(module->types.size()));
      *sig = nullptr;
      return 0;
    }
    *sig = module->signature(sig_index);
    return sig_index;
  }

  uint32_t consume_tag_sig_index(WasmModule* module, const FunctionSig** sig) {
    const byte* pos = pc_;
    uint32_t sig_index = consume_sig_index(module, sig);
    if (*sig && (*sig)->return_count() != 0) {
      errorf(pos, "tag signature %u has non-void return", sig_index);
      *sig = nullptr;
      return 0;
    }
    return sig_index;
  }

  uint32_t consume_count(const char* name, size_t maximum) {
    const byte* p = pc_;
    uint32_t count = consume_u32v(name);
    if (count > maximum) {
      errorf(p, "%s of %u exceeds internal limit of %zu", name, count, maximum);
      return static_cast<uint32_t>(maximum);
    }
    return count;
  }

  uint32_t consume_func_index(WasmModule* module, WasmFunction** func,
                              const char* name) {
    return consume_index(name, &module->functions, func);
  }

  uint32_t consume_global_index(WasmModule* module, WasmGlobal** global) {
    return consume_index("global index", &module->globals, global);
  }

  uint32_t consume_table_index(WasmModule* module, WasmTable** table) {
    return consume_index("table index", &module->tables, table);
  }

  uint32_t consume_tag_index(WasmModule* module, WasmTag** tag) {
    return consume_index("tag index", &module->tags, tag);
  }

  template <typename T>
  uint32_t consume_index(const char* name, std::vector<T>* vector, T** ptr) {
    const byte* pos = pc_;
    uint32_t index = consume_u32v(name);
    if (index >= vector->size()) {
      errorf(pos, "%s %u out of bounds (%d entr%s)", name, index,
             static_cast<int>(vector->size()),
             vector->size() == 1 ? "y" : "ies");
      *ptr = nullptr;
      return 0;
    }
    *ptr = &(*vector)[index];
    return index;
  }

  uint8_t validate_table_flags(const char* name) {
    uint8_t flags = consume_u8("table limits flags");
    STATIC_ASSERT(kNoMaximum < kWithMaximum);
    if (V8_UNLIKELY(flags > kWithMaximum)) {
      errorf(pc() - 1, "invalid %s limits flags", name);
    }
    return flags;
  }

  uint8_t validate_memory_flags(bool* has_shared_memory, bool* is_memory64) {
    uint8_t flags = consume_u8("memory limits flags");
    *has_shared_memory = false;
    switch (flags) {
      case kNoMaximum:
      case kWithMaximum:
        break;
      case kSharedNoMaximum:
      case kSharedWithMaximum:
        if (!enabled_features_.has_threads()) {
          errorf(pc() - 1,
                 "invalid memory limits flags 0x%x (enable via "
                 "--experimental-wasm-threads)",
                 flags);
        }
        *has_shared_memory = true;
        // V8 does not support shared memory without a maximum.
        if (flags == kSharedNoMaximum) {
          errorf(pc() - 1,
                 "memory limits flags must have maximum defined if shared is "
                 "true");
        }
        break;
      case kMemory64NoMaximum:
      case kMemory64WithMaximum:
        if (!enabled_features_.has_memory64()) {
          errorf(pc() - 1,
                 "invalid memory limits flags 0x%x (enable via "
                 "--experimental-wasm-memory64)",
                 flags);
        }
        *is_memory64 = true;
        break;
      default:
        errorf(pc() - 1, "invalid memory limits flags 0x%x", flags);
        break;
    }
    return flags;
  }

  void consume_resizable_limits(const char* name, const char* units,
                                uint32_t max_initial, uint32_t* initial,
                                bool* has_max, uint32_t max_maximum,
                                uint32_t* maximum, uint8_t flags) {
    const byte* pos = pc();
    // For memory64 we need to read the numbers as LEB-encoded 64-bit unsigned
    // integer. All V8 limits are still within uint32_t range though.
    const bool is_memory64 =
        flags == kMemory64NoMaximum || flags == kMemory64WithMaximum;
    uint64_t initial_64 = is_memory64 ? consume_u64v("initial size")
                                      : consume_u32v("initial size");
    if (initial_64 > max_initial) {
      errorf(pos,
             "initial %s size (%" PRIu64
             " %s) is larger than implementation limit (%u)",
             name, initial_64, units, max_initial);
    }
    *initial = static_cast<uint32_t>(initial_64);
    if (flags & 1) {
      *has_max = true;
      pos = pc();
      uint64_t maximum_64 = is_memory64 ? consume_u64v("maximum size")
                                        : consume_u32v("maximum size");
      if (maximum_64 > max_maximum) {
        errorf(pos,
               "maximum %s size (%" PRIu64
               " %s) is larger than implementation limit (%u)",
               name, maximum_64, units, max_maximum);
      }
      if (maximum_64 < *initial) {
        errorf(pos,
               "maximum %s size (%" PRIu64 " %s) is less than initial (%u %s)",
               name, maximum_64, units, *initial, units);
      }
      *maximum = static_cast<uint32_t>(maximum_64);
    } else {
      *has_max = false;
      *maximum = max_initial;
    }
  }

  // Consumes a byte, and emits an error if it does not equal {expected}.
  bool expect_u8(const char* name, uint8_t expected) {
    const byte* pos = pc();
    uint8_t value = consume_u8(name);
    if (value != expected) {
      errorf(pos, "expected %s 0x%02x, got 0x%02x", name, expected, value);
      return false;
    }
    return true;
  }

  ConstantExpression consume_init_expr(WasmModule* module, ValueType expected) {
    uint32_t length;

    // The error message mimics the one generated by the {WasmFullDecoder}.
#define TYPE_CHECK(found)                                             \
  if (V8_UNLIKELY(!IsSubtypeOf(found, expected, module_.get()))) {    \
    errorf(pc() + 1,                                                  \
           "type error in init. expression[0] (expected %s, got %s)", \
           expected.name().c_str(), found.name().c_str());            \
    return {};                                                        \
  }

    // To avoid initializing a {WasmFullDecoder} for the most common
    // expressions, we replicate their decoding and validation here. The
    // manually handled cases correspond to {ConstantExpression}'s kinds.
    // We need to make sure to check that the expression ends in {kExprEnd};
    // otherwise, it is just the first operand of a composite expression, and we
    // fall back to the default case.
    if (!more()) {
      error("Beyond end of code");
      return {};
    }
    switch (static_cast<WasmOpcode>(*pc())) {
      case kExprI32Const: {
        int32_t value =
            read_i32v<kFullValidation>(pc() + 1, &length, "i32.const");
        if (V8_UNLIKELY(failed())) return {};
        if (V8_LIKELY(lookahead(1 + length, kExprEnd))) {
          TYPE_CHECK(kWasmI32)
          consume_bytes(length + 2);
          return ConstantExpression::I32Const(value);
        }
        break;
      }
      case kExprRefFunc: {
        uint32_t index =
            read_u32v<kFullValidation>(pc() + 1, &length, "ref.func");
        if (V8_UNLIKELY(failed())) return {};
        if (V8_LIKELY(lookahead(1 + length, kExprEnd))) {
          if (V8_UNLIKELY(index >= module_->functions.size())) {
            errorf(pc() + 1, "function index %u out of bounds", index);
            return {};
          }
          ValueType type =
              enabled_features_.has_typed_funcref()
                  ? ValueType::Ref(module_->functions[index].sig_index,
                                   kNonNullable)
                  : kWasmFuncRef;
          TYPE_CHECK(type)
          module_->functions[index].declared = true;
          consume_bytes(length + 2);
          return ConstantExpression::RefFunc(index);
        }
        break;
      }
      case kExprRefNull: {
        HeapType type = value_type_reader::read_heap_type<kFullValidation>(
            this, pc() + 1, &length, module_.get(), enabled_features_);
        if (V8_UNLIKELY(failed())) return {};
        if (V8_LIKELY(lookahead(1 + length, kExprEnd))) {
          TYPE_CHECK(ValueType::Ref(type, kNullable))
          consume_bytes(length + 2);
          return ConstantExpression::RefNull(type.representation());
        }
        break;
      }
      default:
        break;
    }
#undef TYPE_CHECK

    auto sig = FixedSizeSignature<ValueType>::Returns(expected);
    FunctionBody body(&sig, buffer_offset_, pc_, end_);
    WasmFeatures detected;
    WasmFullDecoder<Decoder::kFullValidation, InitExprInterface,
                    kInitExpression>
        decoder(&init_expr_zone_, module, enabled_features_, &detected, body,
                module);

    uint32_t offset = this->pc_offset();

    decoder.DecodeFunctionBody();

    this->pc_ = decoder.end();

    if (decoder.failed()) {
      error(decoder.error().offset(), decoder.error().message().c_str());
      return {};
    }

    if (!decoder.interface().end_found()) {
      error("Initializer expression is missing 'end'");
      return {};
    }

    return ConstantExpression::WireBytes(
        offset, static_cast<uint32_t>(decoder.end() - decoder.start()));
  }

  // Read a mutability flag
  bool consume_mutability() {
    byte val = consume_u8("mutability");
    if (val > 1) error(pc_ - 1, "invalid mutability");
    return val != 0;
  }

  ValueType consume_value_type() {
    uint32_t type_length;
    ValueType result = value_type_reader::read_value_type<kFullValidation>(
        this, this->pc(), &type_length, module_.get(),
        origin_ == kWasmOrigin ? enabled_features_ : WasmFeatures::None());
    consume_bytes(type_length, "value type");
    return result;
  }

  HeapType consume_super_type() {
    return value_type_reader::consume_heap_type(this, module_.get(),
                                                enabled_features_);
  }

  ValueType consume_storage_type() {
    uint8_t opcode = read_u8<kFullValidation>(this->pc());
    switch (opcode) {
      case kI8Code:
        consume_bytes(1, "i8");
        return kWasmI8;
      case kI16Code:
        consume_bytes(1, "i16");
        return kWasmI16;
      default:
        // It is not a packed type, so it has to be a value type.
        return consume_value_type();
    }
  }

  // Reads a reference type for tables and element segment headers.
  ValueType consume_reference_type() {
    const byte* position = pc();
    ValueType result = consume_value_type();
    if (!result.is_reference()) {
      error(position, "expected reference type");
    }
    return result;
  }

  const FunctionSig* consume_sig(Zone* zone) {
    // Parse parameter types.
    uint32_t param_count =
        consume_count("param count", kV8MaxWasmFunctionParams);
    if (failed()) return nullptr;
    std::vector<ValueType> params;
    for (uint32_t i = 0; ok() && i < param_count; ++i) {
      params.push_back(consume_value_type());
    }
    std::vector<ValueType> returns;

    // Parse return types.
    uint32_t return_count =
        consume_count("return count", kV8MaxWasmFunctionReturns);
    if (failed()) return nullptr;
    for (uint32_t i = 0; ok() && i < return_count; ++i) {
      returns.push_back(consume_value_type());
    }
    if (failed()) return nullptr;

    // FunctionSig stores the return types first.
    ValueType* buffer = zone->NewArray<ValueType>(param_count + return_count);
    uint32_t b = 0;
    for (uint32_t i = 0; i < return_count; ++i) buffer[b++] = returns[i];
    for (uint32_t i = 0; i < param_count; ++i) buffer[b++] = params[i];

    return zone->New<FunctionSig>(return_count, param_count, buffer);
  }

  const StructType* consume_struct(Zone* zone) {
    uint32_t field_count = consume_count("field count", kV8MaxWasmStructFields);
    if (failed()) return nullptr;
    ValueType* fields = zone->NewArray<ValueType>(field_count);
    bool* mutabilities = zone->NewArray<bool>(field_count);
    for (uint32_t i = 0; ok() && i < field_count; ++i) {
      fields[i] = consume_storage_type();
      mutabilities[i] = consume_mutability();
    }
    if (failed()) return nullptr;
    uint32_t* offsets = zone->NewArray<uint32_t>(field_count);
    return zone->New<StructType>(field_count, offsets, fields, mutabilities);
  }

  const ArrayType* consume_array(Zone* zone) {
    ValueType element_type = consume_storage_type();
    bool mutability = consume_mutability();
    if (failed()) return nullptr;
    return zone->New<ArrayType>(element_type, mutability);
  }

  // Consume the attribute field of an exception.
  uint32_t consume_exception_attribute() {
    const byte* pos = pc_;
    uint32_t attribute = consume_u32v("exception attribute");
    if (attribute != kExceptionAttribute) {
      errorf(pos, "exception attribute %u not supported", attribute);
      return 0;
    }
    return attribute;
  }

  WasmElemSegment consume_element_segment_header() {
    const byte* pos = pc();

    // The mask for the bit in the flag which indicates if the segment is
    // active or not (0 is active).
    constexpr uint8_t kNonActiveMask = 1 << 0;
    // The mask for the bit in the flag which indicates:
    // - for active tables, if the segment has an explicit table index field.
    // - for non-active tables, whether the table is declarative (vs. passive).
    constexpr uint8_t kHasTableIndexOrIsDeclarativeMask = 1 << 1;
    // The mask for the bit in the flag which indicates if the functions of this
    // segment are defined as function indices (0) or init. expressions (1).
    constexpr uint8_t kExpressionsAsElementsMask = 1 << 2;
    constexpr uint8_t kFullMask = kNonActiveMask |
                                  kHasTableIndexOrIsDeclarativeMask |
                                  kExpressionsAsElementsMask;

    uint32_t flag = consume_u32v("flag");
    if ((flag & kFullMask) != flag) {
      errorf(pos, "illegal flag value %u. Must be between 0 and 7", flag);
      return {};
    }

    const WasmElemSegment::Status status =
        (flag & kNonActiveMask) ? (flag & kHasTableIndexOrIsDeclarativeMask)
                                      ? WasmElemSegment::kStatusDeclarative
                                      : WasmElemSegment::kStatusPassive
                                : WasmElemSegment::kStatusActive;
    const bool is_active = status == WasmElemSegment::kStatusActive;

    WasmElemSegment::ElementType element_type =
        flag & kExpressionsAsElementsMask
            ? WasmElemSegment::kExpressionElements
            : WasmElemSegment::kFunctionIndexElements;

    const bool has_table_index =
        is_active && (flag & kHasTableIndexOrIsDeclarativeMask);
    uint32_t table_index = has_table_index ? consume_u32v("table index") : 0;
    if (is_active && table_index >= module_->tables.size()) {
      errorf(pos, "out of bounds%s table index %u",
             has_table_index ? " implicit" : "", table_index);
      return {};
    }
    ValueType table_type =
        is_active ? module_->tables[table_index].type : kWasmBottom;

    ConstantExpression offset;
    if (is_active) {
      offset = consume_init_expr(module_.get(), kWasmI32);
      // Failed to parse offset initializer, return early.
      if (failed()) return {};
    }

    // Denotes an active segment without table index, type, or element kind.
    const bool backwards_compatible_mode =
        is_active && !(flag & kHasTableIndexOrIsDeclarativeMask);
    ValueType type;
    if (element_type == WasmElemSegment::kExpressionElements) {
      type =
          backwards_compatible_mode ? kWasmFuncRef : consume_reference_type();
      if (is_active && !IsSubtypeOf(type, table_type, this->module_.get())) {
        errorf(pos,
               "Element segment of type %s is not a subtype of referenced "
               "table %u (of type %s)",
               type.name().c_str(), table_index, table_type.name().c_str());
        return {};
      }
    } else {
      if (!backwards_compatible_mode) {
        // We have to check that there is an element kind of type Function. All
        // other element kinds are not valid yet.
        uint8_t val = consume_u8("element kind");
        if (static_cast<ImportExportKindCode>(val) != kExternalFunction) {
          errorf(pos, "illegal element kind 0x%x. Must be 0x%x", val,
                 kExternalFunction);
          return {};
        }
      }
      if (!is_active) {
        // Declarative and passive segments without explicit type are funcref.
        type = kWasmFuncRef;
      } else {
        type = table_type;
        // Active segments with function indices must reference a function
        // table. TODO(7748): Add support for anyref tables when we have them.
        if (!IsSubtypeOf(table_type, kWasmFuncRef, this->module_.get())) {
          errorf(pos,
                 "An active element segment with function indices as elements "
                 "must reference a table of %s. Instead, table %u of type %s "
                 "is referenced.",
                 enabled_features_.has_typed_funcref()
                     ? "a subtype of type funcref"
                     : "type funcref",
                 table_index, table_type.name().c_str());
          return {};
        }
      }
    }

    if (is_active) {
      return {type, table_index, std::move(offset), element_type};
    } else {
      return {type, status, element_type};
    }
  }

  void consume_data_segment_header(bool* is_active, uint32_t* index,
                                   ConstantExpression* offset) {
    const byte* pos = pc();
    uint32_t flag = consume_u32v("flag");

    // Some flag values are only valid for specific proposals.
    if (flag != SegmentFlags::kActiveNoIndex &&
        flag != SegmentFlags::kPassive &&
        flag != SegmentFlags::kActiveWithIndex) {
      errorf(pos, "illegal flag value %u. Must be 0, 1, or 2", flag);
      return;
    }

    // We know now that the flag is valid. Time to read the rest.
    ValueType expected_type = module_->is_memory64 ? kWasmI64 : kWasmI32;
    if (flag == SegmentFlags::kActiveNoIndex) {
      *is_active = true;
      *index = 0;
      *offset = consume_init_expr(module_.get(), expected_type);
      return;
    }
    if (flag == SegmentFlags::kPassive) {
      *is_active = false;
      return;
    }
    if (flag == SegmentFlags::kActiveWithIndex) {
      *is_active = true;
      *index = consume_u32v("memory index");
      *offset = consume_init_expr(module_.get(), expected_type);
    }
  }

  uint32_t consume_element_func_index(ValueType expected) {
    WasmFunction* func = nullptr;
    const byte* initial_pc = pc();
    uint32_t index =
        consume_func_index(module_.get(), &func, "element function index");
    if (failed()) return index;
    DCHECK_NOT_NULL(func);
    DCHECK_EQ(index, func->func_index);
    ValueType entry_type = ValueType::Ref(func->sig_index, kNonNullable);
    if (V8_UNLIKELY(!IsSubtypeOf(entry_type, expected, module_.get()))) {
      errorf(initial_pc,
             "Invalid type in element entry: expected %s, got %s instead.",
             expected.name().c_str(), entry_type.name().c_str());
      return index;
    }
    func->declared = true;
    return index;
  }
};

ModuleResult DecodeWasmModule(
    const WasmFeatures& enabled, const byte* module_start,
    const byte* module_end, bool verify_functions, ModuleOrigin origin,
    Counters* counters, std::shared_ptr<metrics::Recorder> metrics_recorder,
    v8::metrics::Recorder::ContextId context_id, DecodingMethod decoding_method,
    AccountingAllocator* allocator) {
  size_t size = module_end - module_start;
  CHECK_LE(module_start, module_end);
  size_t max_size = max_module_size();
  if (size > max_size) {
    return ModuleResult{
        WasmError{0, "size > maximum module size (%zu): %zu", max_size, size}};
  }
  // TODO(bradnelson): Improve histogram handling of size_t.
  auto size_counter =
      SELECT_WASM_COUNTER(counters, origin, wasm, module_size_bytes);
  size_counter->AddSample(static_cast<int>(size));
  // Signatures are stored in zone memory, which have the same lifetime
  // as the {module}.
  ModuleDecoderImpl decoder(enabled, module_start, module_end, origin);
  v8::metrics::WasmModuleDecoded metrics_event;
  base::ElapsedTimer timer;
  timer.Start();
  base::ThreadTicks thread_ticks = base::ThreadTicks::IsSupported()
                                       ? base::ThreadTicks::Now()
                                       : base::ThreadTicks();
  ModuleResult result =
      decoder.DecodeModule(counters, allocator, verify_functions);

  // Record event metrics.
  metrics_event.wall_clock_duration_in_us = timer.Elapsed().InMicroseconds();
  timer.Stop();
  if (!thread_ticks.IsNull()) {
    metrics_event.cpu_duration_in_us =
        (base::ThreadTicks::Now() - thread_ticks).InMicroseconds();
  }
  metrics_event.success = decoder.ok() && result.ok();
  metrics_event.async = decoding_method == DecodingMethod::kAsync ||
                        decoding_method == DecodingMethod::kAsyncStream;
  metrics_event.streamed = decoding_method == DecodingMethod::kSyncStream ||
                           decoding_method == DecodingMethod::kAsyncStream;
  if (result.ok()) {
    metrics_event.function_count = result.value()->num_declared_functions;
  } else if (auto&& module = decoder.shared_module()) {
    metrics_event.function_count = module->num_declared_functions;
  }
  metrics_event.module_size_in_bytes = size;
  metrics_recorder->DelayMainThreadEvent(metrics_event, context_id);

  return result;
}

ModuleDecoder::ModuleDecoder(const WasmFeatures& enabled)
    : enabled_features_(enabled) {}

ModuleDecoder::~ModuleDecoder() = default;

const std::shared_ptr<WasmModule>& ModuleDecoder::shared_module() const {
  return impl_->shared_module();
}

void ModuleDecoder::StartDecoding(
    Counters* counters, std::shared_ptr<metrics::Recorder> metrics_recorder,
    v8::metrics::Recorder::ContextId context_id, AccountingAllocator* allocator,
    ModuleOrigin origin) {
  DCHECK_NULL(impl_);
  impl_.reset(new ModuleDecoderImpl(enabled_features_, origin));
  impl_->StartDecoding(counters, allocator);
}

void ModuleDecoder::DecodeModuleHeader(base::Vector<const uint8_t> bytes,
                                       uint32_t offset) {
  impl_->DecodeModuleHeader(bytes, offset);
}

void ModuleDecoder::DecodeSection(SectionCode section_code,
                                  base::Vector<const uint8_t> bytes,
                                  uint32_t offset, bool verify_functions) {
  impl_->DecodeSection(section_code, bytes, offset, verify_functions);
}

void ModuleDecoder::DecodeFunctionBody(uint32_t index, uint32_t length,
                                       uint32_t offset, bool verify_functions) {
  impl_->DecodeFunctionBody(index, length, offset, verify_functions);
}

void ModuleDecoder::StartCodeSection() { impl_->StartCodeSection(); }

bool ModuleDecoder::CheckFunctionsCount(uint32_t functions_count,
                                        uint32_t error_offset) {
  return impl_->CheckFunctionsCount(functions_count, error_offset);
}

ModuleResult ModuleDecoder::FinishDecoding(bool verify_functions) {
  return impl_->FinishDecoding(verify_functions);
}

void ModuleDecoder::set_code_section(uint32_t offset, uint32_t size) {
  return impl_->set_code_section(offset, size);
}

size_t ModuleDecoder::IdentifyUnknownSection(ModuleDecoder* decoder,
                                             base::Vector<const uint8_t> bytes,
                                             uint32_t offset,
                                             SectionCode* result) {
  if (!decoder->ok()) return 0;
  decoder->impl_->Reset(bytes, offset);
  *result = IdentifyUnknownSectionInternal(decoder->impl_.get());
  return decoder->impl_->pc() - bytes.begin();
}

bool ModuleDecoder::ok() { return impl_->ok(); }

const FunctionSig* DecodeWasmSignatureForTesting(const WasmFeatures& enabled,
                                                 Zone* zone, const byte* start,
                                                 const byte* end) {
  ModuleDecoderImpl decoder(enabled, start, end, kWasmOrigin);
  return decoder.DecodeFunctionSignature(zone, start);
}

ConstantExpression DecodeWasmInitExprForTesting(const WasmFeatures& enabled,
                                                const byte* start,
                                                const byte* end,
                                                ValueType expected) {
  ModuleDecoderImpl decoder(enabled, start, end, kWasmOrigin);
  AccountingAllocator allocator;
  decoder.StartDecoding(nullptr, &allocator);
  return decoder.DecodeInitExprForTesting(expected);
}

FunctionResult DecodeWasmFunctionForTesting(
    const WasmFeatures& enabled, Zone* zone, const ModuleWireBytes& wire_bytes,
    const WasmModule* module, const byte* function_start,
    const byte* function_end, Counters* counters) {
  size_t size = function_end - function_start;
  CHECK_LE(function_start, function_end);
  if (size > kV8MaxWasmFunctionSize) {
    return FunctionResult{WasmError{0,
                                    "size > maximum function size (%zu): %zu",
                                    kV8MaxWasmFunctionSize, size}};
  }
  ModuleDecoderImpl decoder(enabled, function_start, function_end, kWasmOrigin);
  decoder.SetCounters(counters);
  return decoder.DecodeSingleFunction(zone, wire_bytes, module,
                                      std::make_unique<WasmFunction>());
}

AsmJsOffsetsResult DecodeAsmJsOffsets(
    base::Vector<const uint8_t> encoded_offsets) {
  std::vector<AsmJsOffsetFunctionEntries> functions;

  Decoder decoder(encoded_offsets);
  uint32_t functions_count = decoder.consume_u32v("functions count");
  // Consistency check.
  DCHECK_GE(encoded_offsets.size(), functions_count);
  functions.reserve(functions_count);

  for (uint32_t i = 0; i < functions_count; ++i) {
    uint32_t size = decoder.consume_u32v("table size");
    if (size == 0) {
      functions.emplace_back();
      continue;
    }
    DCHECK(decoder.checkAvailable(size));
    const byte* table_end = decoder.pc() + size;
    uint32_t locals_size = decoder.consume_u32v("locals size");
    int function_start_position = decoder.consume_u32v("function start pos");
    int function_end_position = function_start_position;
    int last_byte_offset = locals_size;
    int last_asm_position = function_start_position;
    std::vector<AsmJsOffsetEntry> func_asm_offsets;
    func_asm_offsets.reserve(size / 4);  // conservative estimation
    // Add an entry for the stack check, associated with position 0.
    func_asm_offsets.push_back(
        {0, function_start_position, function_start_position});
    while (decoder.pc() < table_end) {
      DCHECK(decoder.ok());
      last_byte_offset += decoder.consume_u32v("byte offset delta");
      int call_position =
          last_asm_position + decoder.consume_i32v("call position delta");
      int to_number_position =
          call_position + decoder.consume_i32v("to_number position delta");
      last_asm_position = to_number_position;
      if (decoder.pc() == table_end) {
        // The last entry is the function end marker.
        DCHECK_EQ(call_position, to_number_position);
        function_end_position = call_position;
      } else {
        func_asm_offsets.push_back(
            {last_byte_offset, call_position, to_number_position});
      }
    }
    DCHECK_EQ(decoder.pc(), table_end);
    functions.emplace_back(AsmJsOffsetFunctionEntries{
        function_start_position, function_end_position,
        std::move(func_asm_offsets)});
  }
  DCHECK(decoder.ok());
  DCHECK(!decoder.more());

  return decoder.toResult(AsmJsOffsets{std::move(functions)});
}

std::vector<CustomSectionOffset> DecodeCustomSections(const byte* start,
                                                      const byte* end) {
  Decoder decoder(start, end);
  decoder.consume_bytes(4, "wasm magic");
  decoder.consume_bytes(4, "wasm version");

  std::vector<CustomSectionOffset> result;

  while (decoder.more()) {
    byte section_code = decoder.consume_u8("section code");
    uint32_t section_length = decoder.consume_u32v("section length");
    uint32_t section_start = decoder.pc_offset();
    if (section_code != 0) {
      // Skip known sections.
      decoder.consume_bytes(section_length, "section bytes");
      continue;
    }
    uint32_t name_length = decoder.consume_u32v("name length");
    uint32_t name_offset = decoder.pc_offset();
    decoder.consume_bytes(name_length, "section name");
    uint32_t payload_offset = decoder.pc_offset();
    if (section_length < (payload_offset - section_start)) {
      decoder.error("invalid section length");
      break;
    }
    uint32_t payload_length = section_length - (payload_offset - section_start);
    decoder.consume_bytes(payload_length);
    if (decoder.failed()) break;
    result.push_back({{section_start, section_length},
                      {name_offset, name_length},
                      {payload_offset, payload_length}});
  }

  return result;
}

namespace {

bool FindNameSection(Decoder* decoder) {
  static constexpr int kModuleHeaderSize = 8;
  decoder->consume_bytes(kModuleHeaderSize, "module header");

  WasmSectionIterator section_iter(decoder);

  while (decoder->ok() && section_iter.more() &&
         section_iter.section_code() != kNameSectionCode) {
    section_iter.advance(true);
  }
  if (!section_iter.more()) return false;

  // Reset the decoder to not read beyond the name section end.
  decoder->Reset(section_iter.payload(), decoder->pc_offset());
  return true;
}

}  // namespace

void DecodeFunctionNames(const byte* module_start, const byte* module_end,
                         std::unordered_map<uint32_t, WireBytesRef>* names) {
  DCHECK_NOT_NULL(names);
  DCHECK(names->empty());

  Decoder decoder(module_start, module_end);
  if (FindNameSection(&decoder)) {
    while (decoder.ok() && decoder.more()) {
      uint8_t name_type = decoder.consume_u8("name type");
      if (name_type & 0x80) break;  // no varuint7

      uint32_t name_payload_len = decoder.consume_u32v("name payload length");
      if (!decoder.checkAvailable(name_payload_len)) break;

      if (name_type != NameSectionKindCode::kFunctionCode) {
        decoder.consume_bytes(name_payload_len, "name subsection payload");
        continue;
      }
      uint32_t functions_count = decoder.consume_u32v("functions count");

      for (; decoder.ok() && functions_count > 0; --functions_count) {
        uint32_t function_index = decoder.consume_u32v("function index");
        WireBytesRef name = consume_string(&decoder, false, "function name");

        // Be lenient with errors in the name section: Ignore non-UTF8 names.
        // You can even assign to the same function multiple times (last valid
        // one wins).
        if (decoder.ok() && validate_utf8(&decoder, name)) {
          names->insert(std::make_pair(function_index, name));
        }
      }
    }
  }
}

NameMap DecodeNameMap(base::Vector<const uint8_t> module_bytes,
                      uint8_t name_section_kind) {
  Decoder decoder(module_bytes);
  if (!FindNameSection(&decoder)) return NameMap{{}};

  std::vector<NameAssoc> names;
  while (decoder.ok() && decoder.more()) {
    uint8_t name_type = decoder.consume_u8("name type");
    if (name_type & 0x80) break;  // no varuint7

    uint32_t name_payload_len = decoder.consume_u32v("name payload length");
    if (!decoder.checkAvailable(name_payload_len)) break;

    if (name_type != name_section_kind) {
      decoder.consume_bytes(name_payload_len, "name subsection payload");
      continue;
    }

    uint32_t count = decoder.consume_u32v("names count");
    for (uint32_t i = 0; i < count; i++) {
      uint32_t index = decoder.consume_u32v("index");
      WireBytesRef name = consume_string(&decoder, false, "name");
      if (!decoder.ok()) break;
      if (index > kMaxInt) continue;
      if (!validate_utf8(&decoder, name)) continue;
      names.emplace_back(static_cast<int>(index), name);
    }
  }
  std::stable_sort(names.begin(), names.end(), NameAssoc::IndexLess{});
  return NameMap{std::move(names)};
}

IndirectNameMap DecodeIndirectNameMap(base::Vector<const uint8_t> module_bytes,
                                      uint8_t name_section_kind) {
  Decoder decoder(module_bytes);
  if (!FindNameSection(&decoder)) return IndirectNameMap{{}};

  std::vector<IndirectNameMapEntry> entries;
  while (decoder.ok() && decoder.more()) {
    uint8_t name_type = decoder.consume_u8("name type");
    if (name_type & 0x80) break;  // no varuint7

    uint32_t name_payload_len = decoder.consume_u32v("name payload length");
    if (!decoder.checkAvailable(name_payload_len)) break;

    if (name_type != name_section_kind) {
      decoder.consume_bytes(name_payload_len, "name subsection payload");
      continue;
    }

    uint32_t outer_count = decoder.consume_u32v("outer count");
    for (uint32_t i = 0; i < outer_count; ++i) {
      uint32_t outer_index = decoder.consume_u32v("outer index");
      if (outer_index > kMaxInt) continue;
      std::vector<NameAssoc> names;
      uint32_t inner_count = decoder.consume_u32v("inner count");
      for (uint32_t k = 0; k < inner_count; ++k) {
        uint32_t inner_index = decoder.consume_u32v("inner index");
        WireBytesRef name = consume_string(&decoder, false, "name");
        if (!decoder.ok()) break;
        if (inner_index > kMaxInt) continue;
        // Ignore non-utf8 names.
        if (!validate_utf8(&decoder, name)) continue;
        names.emplace_back(static_cast<int>(inner_index), name);
      }
      // Use stable sort to get deterministic names (the first one declared)
      // even in the presence of duplicates.
      std::stable_sort(names.begin(), names.end(), NameAssoc::IndexLess{});
      entries.emplace_back(static_cast<int>(outer_index), std::move(names));
    }
  }
  std::stable_sort(entries.begin(), entries.end(),
                   IndirectNameMapEntry::IndexLess{});
  return IndirectNameMap{std::move(entries)};
}

#undef TRACE

}  // namespace wasm
}  // namespace internal
}  // namespace v8
