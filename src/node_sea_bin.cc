#include "node_sea.h"

#ifdef HAVE_LIEF
#include "LIEF/LIEF.hpp"
#endif  // HAVE_LIEF

#include "debug_utils-inl.h"
#include "env-inl.h"
#include "node_exit_code.h"
#include "simdutf.h"
#include "util-inl.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

// The POSTJECT_SENTINEL_FUSE macro is a string of random characters selected by
// the Node.js project that is present only once in the entire binary. It is
// used by the postject_has_resource() function to efficiently detect if a
// resource has been injected. See
// https://github.com/nodejs/postject/blob/35343439cac8c488f2596d7c4c1dddfec1fddcae/postject-api.h#L42-L45.
#define POSTJECT_SENTINEL_FUSE "NODE_SEA_FUSE_fce680ab2cc467b6e072b8b5df1996b2"
#ifdef HAVE_LIEF
static constexpr const char* kSentinelFusePart1 = "NODE_SEA_FUSE";
static constexpr const char* kSentinelFusePart2 =
    "fce680ab2cc467b6e072b8b5df1996b2";
#endif  // HAVE_LIEF
#include "postject-api.h"
#undef POSTJECT_SENTINEL_FUSE

namespace node {
namespace sea {

// TODO(joyeecheung): use LIEF to locate it directly.
std::string_view FindSingleExecutableBlob() {
#if !defined(DISABLE_SINGLE_EXECUTABLE_APPLICATION)
  CHECK(IsSingleExecutable());
  static const std::string_view result = []() -> std::string_view {
    size_t size;
#ifdef __APPLE__
    postject_options options;
    postject_options_init(&options);
    options.macho_segment_name = "NODE_SEA";
    const char* blob = static_cast<const char*>(
        postject_find_resource("NODE_SEA_BLOB", &size, &options));
#else
    const char* blob = static_cast<const char*>(
        postject_find_resource("NODE_SEA_BLOB", &size, nullptr));
#endif
    return {blob, size};
  }();
  per_process::Debug(DebugCategory::SEA,
                     "Found SEA blob %p, size=%zu\n",
                     result.data(),
                     result.size());
  return result;
#else
  UNREACHABLE();
#endif  // !defined(DISABLE_SINGLE_EXECUTABLE_APPLICATION)
}

bool IsSingleExecutable() {
  return postject_has_resource();
}

#ifdef HAVE_LIEF
static constexpr const char* kSEAResourceName = "NODE_SEA_BLOB";
static constexpr const char* kELFSectionName = ".note.node.sea";
static constexpr const char* kMachoSegmentName = "NODE_SEA";

enum class InjectResult { kAlreadyExists, kError, kSuccess, kUnknownFormat };

struct InjectOutput {
  InjectResult result;
  std::vector<uint8_t> data;  // Empty if result != kSuccess
};

InjectOutput InjectIntoElf(const std::vector<uint8_t>& executable,
                           const std::string& note_name,
                           const std::vector<uint8_t>& data,
                           bool overwrite = false) {
  per_process::Debug(DebugCategory::SEA,
                     "Parsing ELF binary for injection...\n");
  std::unique_ptr<LIEF::ELF::Binary> binary =
      LIEF::ELF::Parser::parse(executable);
  if (!binary) {
    FPrintF(stderr, "Failed to parse ELF binary\n");
    return {InjectResult::kError, {}};
  }

  constexpr uint32_t kNoteType = 0;  // vendor-specific

  LIEF::ELF::Note* existing_note = nullptr;
  per_process::Debug(DebugCategory::SEA,
                     "Searching for existing note %s (type: %d)\n",
                     note_name,
                     kNoteType);
  for (const auto& n : binary->notes()) {
    if (n.name() == note_name && static_cast<uint32_t>(n.type()) == kNoteType) {
      existing_note = n.clone().release();
      break;
    }
  }

  if (existing_note) {
    per_process::Debug(DebugCategory::SEA,
                       "Found existing note %s (type: %d)\n",
                       existing_note->name(),
                       static_cast<uint32_t>(existing_note->type()));
    if (!overwrite) {
      FPrintF(stderr,
              "Error: note section %s already exists in the ELF executable.\n"
              "Use \"overwriteExisting\": true in the configuration to "
              "overwrite.\n",
              note_name);
      return {InjectResult::kAlreadyExists, {}};
    }
    per_process::Debug(DebugCategory::SEA,
                       "Removing existing note before injection.\n");
    binary->remove(*existing_note);
  } else {
    per_process::Debug(DebugCategory::SEA,
                       "No existing note found. Proceeding to add new note.\n");
  }

  auto new_note =
      LIEF::ELF::Note::create(note_name, kNoteType, data, kELFSectionName);
  if (!new_note) {
    FPrintF(stderr, "Failed to create new ELF note %s\n", note_name);
    return {InjectResult::kError, {}};
  }
  binary->add(*new_note);

  LIEF::ELF::Builder::config_t cfg;
  cfg.notes = true;            // Ensure notes are rebuilt
  cfg.dynamic_section = true;  // Ensure PT_DYNAMIC is rebuilt

  per_process::Debug(DebugCategory::SEA,
                     "Building modified ELF binary with new note...\n");
  LIEF::ELF::Builder builder(*binary, cfg);
  builder.build();
  if (builder.get_build().empty()) {
    FPrintF(stderr, "Failed to build modified ELF binary\n");
    return {InjectResult::kError, {}};
  }
  return InjectOutput{InjectResult::kSuccess, builder.get_build()};
}

InjectOutput InjectIntoMacho(const std::vector<uint8_t>& executable,
                             const std::string& segment_name,
                             const std::string& section_name,
                             const std::vector<uint8_t>& data,
                             bool overwrite = false) {
  per_process::Debug(DebugCategory::SEA,
                     "Parsing Mach-O binary for injection...\n");
  std::unique_ptr<LIEF::MachO::FatBinary> fat_binary =
      LIEF::MachO::Parser::parse(executable);
  if (!fat_binary) {
    FPrintF(stderr, "Failed to parse Mach-O binary\n");
    return {InjectResult::kError, {}};
  }

  // Inject into all Mach-O binaries if there's more than one in a fat binary
  per_process::Debug(DebugCategory::SEA,
                     "Searching for existing section %s/%s\n",
                     segment_name,
                     section_name);
  for (auto& binary : *fat_binary) {
    LIEF::MachO::Section* existing_section =
        binary.get_section(segment_name, section_name);
    if (existing_section) {
      if (!overwrite) {
        FPrintF(stderr,
                "Error: Segment/section %s/%s already exists in the Mach-O "
                "executable.\n"
                "Use \"overwriteExisting\": true in the configuration to "
                "overwrite.\n",
                segment_name,
                section_name);
        return {InjectResult::kAlreadyExists, {}};
      }
      per_process::Debug(DebugCategory::SEA,
                         "Removing existing section %s/%s\n",
                         segment_name,
                         section_name);
      binary.remove_section(segment_name, section_name, true);
    }

    LIEF::MachO::SegmentCommand* segment = binary.get_segment(segment_name);
    LIEF::MachO::Section section(section_name, data);
    if (!segment) {
      // Create the segment and mark it read-only
      LIEF::MachO::SegmentCommand new_segment(segment_name);
      // Use SegmentCommand::VM_PROTECTIONS enum values (READ)
      new_segment.max_protection(static_cast<uint32_t>(
          LIEF::MachO::SegmentCommand::VM_PROTECTIONS::READ));
      new_segment.init_protection(static_cast<uint32_t>(
          LIEF::MachO::SegmentCommand::VM_PROTECTIONS::READ));
      new_segment.add_section(section);
      binary.add(new_segment);
    } else {
      binary.add_section(*segment, section);
    }

    // It will need to be signed again anyway, so remove the signature
    if (binary.has_code_signature()) {
      per_process::Debug(DebugCategory::SEA,
                         "Removing existing code signature\n");
      if (binary.remove_signature()) {
        per_process::Debug(DebugCategory::SEA,
                           "Code signature removed successfully\n");
      } else {
        FPrintF(stderr, "Failed to remove existing code signature\n");
        return {InjectResult::kError, {}};
      }
    }
  }

  return InjectOutput{InjectResult::kSuccess, fat_binary->raw()};
}

InjectOutput InjectIntoPe(const std::vector<uint8_t>& executable,
                          const std::string& resource_name,
                          const std::vector<uint8_t>& data,
                          bool overwrite = false) {
  per_process::Debug(DebugCategory::SEA,
                     "Parsing PE binary for injection...\n");
  std::unique_ptr<LIEF::PE::Binary> binary =
      LIEF::PE::Parser::parse(executable);
  if (!binary) {
    FPrintF(stderr, "Failed to parse PE binary\n");
    return {InjectResult::kError, {}};
  }

  // TODO(XXX) - lief.PE.ResourcesManager doesn't support RCDATA it seems, add
  // support so this is simpler?
  if (!binary->has_resources()) {
    // TODO(XXX) - Handle this edge case by creating the resource tree
    FPrintF(stderr, "PE binary has no resources, cannot inject\n");
    return {InjectResult::kError, {}};
  }

  LIEF::PE::ResourceNode* resources = binary->resources();
  LIEF::PE::ResourceNode* rcdata_node = nullptr;
  LIEF::PE::ResourceNode* id_node = nullptr;

  // First level => Type (ResourceDirectory node)
  per_process::Debug(DebugCategory::SEA,
                     "Locating/creating RCDATA resource node\n");
  constexpr uint32_t RCDATA_ID =
      static_cast<uint32_t>(LIEF::PE::ResourcesManager::TYPE::RCDATA);
  auto rcdata_node_iter = std::find_if(std::begin(resources->childs()),
                                       std::end(resources->childs()),
                                       [](const LIEF::PE::ResourceNode& node) {
                                         return node.id() == RCDATA_ID;
                                       });
  if (rcdata_node_iter != std::end(resources->childs())) {
    per_process::Debug(DebugCategory::SEA,
                       "Found existing RCDATA resource node\n");
    rcdata_node = &*rcdata_node_iter;
  } else {
    per_process::Debug(DebugCategory::SEA,
                       "Creating new RCDATA resource node\n");
    LIEF::PE::ResourceDirectory new_rcdata_node;
    new_rcdata_node.id(RCDATA_ID);
    rcdata_node = &resources->add_child(new_rcdata_node);
  }

  // Second level => ID (ResourceDirectory node)
  per_process::Debug(DebugCategory::SEA,
                     "Locating/creating ID resource node for %s\n",
                     resource_name);
  DCHECK(simdutf::validate_ascii(resource_name.data(), resource_name.size()));
  std::u16string resource_name_u16(resource_name.begin(), resource_name.end());
  auto id_node_iter =
      std::find_if(std::begin(rcdata_node->childs()),
                   std::end(rcdata_node->childs()),
                   [resource_name_u16](const LIEF::PE::ResourceNode& node) {
                     return node.name() == resource_name_u16;
                   });
  if (id_node_iter != std::end(rcdata_node->childs())) {
    per_process::Debug(DebugCategory::SEA,
                       "Found existing ID resource node for %s\n",
                       resource_name);
    id_node = &*id_node_iter;
  } else {
    // TODO(XXX) - This isn't documented, but if this isn't set then
    //        LIEF won't save the name. Seems like LIEF should be able
    //        to automatically handle this if you've set the node's name
    per_process::Debug(DebugCategory::SEA,
                       "Creating new ID resource node for %s\n",
                       resource_name);
    LIEF::PE::ResourceDirectory new_id_node;
    new_id_node.name(resource_name);
    new_id_node.id(0x80000000);
    id_node = &rcdata_node->add_child(new_id_node);
  }

  // Third level => Lang (ResourceData node)
  per_process::Debug(DebugCategory::SEA,
                     "Locating existing language resource node for %s\n",
                     resource_name);
  if (id_node->childs() != std::end(id_node->childs())) {
    per_process::Debug(DebugCategory::SEA,
                       "Found existing language resource node for %s\n",
                       resource_name);
    if (!overwrite) {
      FPrintF(stderr,
              "Error: Resource %s exists in the PE executable.\n"
              "Use \"overwriteExisting\": true in the configuration to "
              "overwrite.\n",
              resource_name.c_str());
      return {InjectResult::kAlreadyExists, {}};
    }
    per_process::Debug(DebugCategory::SEA,
                       "Removing existing language resource node for %s\n",
                       resource_name);
    id_node->delete_child(*id_node->childs());
  }

  LIEF::PE::ResourceData lang_node(data);
  id_node->add_child(lang_node);

  per_process::Debug(DebugCategory::SEA,
                     "Rebuilding PE resources with new data for %s\n",
                     resource_name);
  // Write out the binary, only modifying the resources
  LIEF::PE::Builder::config_t cfg;  // TODO(joyeecheung): check the defaults.
  cfg.resources = true;
  cfg.rsrc_section = ".rsrc";  // ensure section name
  LIEF::PE::Builder builder(*binary, cfg);
  if (!builder.build()) {
    FPrintF(stderr, "Failed to build modified PE binary\n");
    return {InjectResult::kError, {}};
  }

  return InjectOutput{InjectResult::kSuccess, builder.get_build()};
}

bool MarkSentinel(std::vector<uint8_t>& data,
                  const std::string& sentinel_fuse) {
  std::string_view fuse(sentinel_fuse);
  std::string_view data_view(reinterpret_cast<char*>(data.data()), data.size());

  size_t first_pos = data_view.find(fuse);
  per_process::Debug(
      DebugCategory::SEA, "Searching for fuse: %s\n", sentinel_fuse);
  if (first_pos == std::string::npos) {
    FPrintF(stderr, "Error: sentinel %s not found\n", sentinel_fuse);
    return false;
  }

  size_t last_pos = data_view.rfind(fuse);
  if (first_pos != last_pos) {
    FPrintF(stderr,
            "Error: found more than one occurrence of sentinel %s\n",
            sentinel_fuse);
    return false;
  }

  size_t colon_pos = first_pos + fuse.size();
  if (colon_pos >= data_view.size() || data_view[colon_pos] != ':') {
    FPrintF(stderr, "Error: missing ':' after sentinel %s\n", sentinel_fuse);
    return false;
  }

  size_t idx = colon_pos + 1;
  // Expecting ':0' or ':1' after the fuse
  if (idx >= data_view.size()) {
    FPrintF(stderr, "Error: sentinel index out of range\n");
    return false;
  }

  per_process::Debug(DebugCategory::SEA,
                     "Found fuse: %s\n",
                     data_view.substr(first_pos, fuse.size() + 2));

  if (data_view[idx] == '0') {
    per_process::Debug(DebugCategory::SEA, "Marking sentinel as 1\n");
    data.data()[idx] = '1';
  } else if (data_view[idx] == '1') {
    per_process::Debug(DebugCategory::SEA, "Sentinel already marked as 1\n");
  } else {
    FPrintF(stderr, "Error: invalid sentinel value %d\n", data_view[idx]);
    return false;
  }
  per_process::Debug(DebugCategory::SEA,
                     "Processed fuse: %s\n",
                     data_view.substr(first_pos, fuse.size() + 2));

  return true;
}

InjectOutput InjectResource(const std::vector<uint8_t>& exe,
                            const std::string& resource_name,
                            const std::vector<uint8_t>& res,
                            const std::string& macho_segment_name,
                            bool overwrite) {
  if (LIEF::ELF::is_elf(exe)) {
    return InjectIntoElf(exe, resource_name, res, overwrite);
  } else if (LIEF::MachO::is_macho(exe)) {
    std::string sec = resource_name;
    if (!(sec.rfind("__", 0) == 0)) sec = "__" + sec;
    return InjectIntoMacho(exe, macho_segment_name, sec, res, overwrite);
  } else if (LIEF::PE::is_pe(exe)) {
    std::string upper_name = resource_name;
    // Convert resource name to uppercase as PE resource names are
    // case-insensitive.
    std::transform(upper_name.begin(),
                   upper_name.end(),
                   upper_name.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return InjectIntoPe(exe, upper_name, res, overwrite);
  }

  fprintf(stderr,
          "Error: Executable must be a supported format: ELF, PE, or Mach-O\n");
  return {InjectResult::kUnknownFormat, {}};
}

ExitCode BuildSingleExecutable(const std::string& sea_config_path,
                               const std::vector<std::string>& args,
                               const std::vector<std::string>& exec_args) {
  std::optional<SeaConfig> opt_config =
      ParseSingleExecutableConfig(sea_config_path);
  if (!opt_config.has_value()) {
    return ExitCode::kGenericUserError;
  }

  SeaConfig config = opt_config.value();
  if (config.executable_path.empty()) {
    config.executable_path = args[0];
  }

  // Get file permissions from source executable to copy over later.
  uv_fs_t req;
  int r = uv_fs_stat(nullptr, &req, config.executable_path.c_str(), nullptr);
  if (r != 0) {
    FPrintF(stderr,
            "Error: Couldn't stat executable: %s\n",
            config.executable_path);
    uv_fs_req_cleanup(&req);
    return ExitCode::kGenericUserError;
  }
  int src_mode = static_cast<int>(req.statbuf.st_mode);
  uv_fs_req_cleanup(&req);

  std::string exe;
  r = ReadFileSync(&exe, config.executable_path.c_str());
  if (r != 0) {
    FPrintF(stderr,
            "Error: Couldn't read executable: %s\n",
            config.executable_path);
    return ExitCode::kGenericUserError;
  }

  // TODO(joyeecheung): add a variant of ReadFileSync that reads into
  // vector<uint8_t> directly and avoid this copy.
  std::vector<uint8_t> exe_data(exe.begin(), exe.end());
  std::vector<char> sea_blob;
  ExitCode code =
      GenerateSingleExecutableBlob(&sea_blob, config, args, exec_args);
  if (code != ExitCode::kNoFailure) {
    return code;
  }
  // TODO(joyeecheung): refactor serializer implementation and avoid copying
  std::vector<uint8_t> sea_blob_u8(sea_blob.begin(), sea_blob.end());
  // For backward compatibility with postject, we construct the sentinel fuse
  // at runtime instead using a constant.
  std::string fuse =
      std::string(kSentinelFusePart1) + "_" + std::string(kSentinelFusePart2);
  InjectOutput out = InjectResource(exe_data,
                                    kSEAResourceName,
                                    sea_blob_u8,
                                    kMachoSegmentName,
                                    config.overwrite_existing);
  if (out.result == InjectResult::kSuccess && !MarkSentinel(out.data, fuse)) {
    out.result = InjectResult::kError;
  }
  if (out.result != InjectResult::kSuccess) {
    return ExitCode::kGenericUserError;
  }

  uv_buf_t buf = uv_buf_init(reinterpret_cast<char*>(out.data.data()),
                             static_cast<size_t>(out.data.size()));
  r = WriteFileSync(config.output_path.c_str(), buf);
  if (r != 0) {
    FPrintF(stderr,
            "Error: Couldn't write output executable: %s: %s\n",
            config.output_path,
            uv_strerror(r));
    return ExitCode::kGenericUserError;
  }

  // Copy file permissions (including execute bit) from source executable
  r = uv_fs_chmod(nullptr, &req, config.output_path.c_str(), src_mode, nullptr);
  uv_fs_req_cleanup(&req);
  if (r != 0) {
    FPrintF(stderr,
            "Warning: Couldn't set permissions %d on %s: %s\n",
            src_mode,
            config.output_path,
            uv_strerror(r));
  }

  FPrintF(stdout,
          "Generated single executable %s + %s -> %s\n",
          config.executable_path,
          sea_config_path,
          config.output_path);
  return ExitCode::kNoFailure;
}
#else
ExitCode BuildSingleExecutable(const std::string& sea_config_path,
                               const std::vector<std::string>& args,
                               const std::vector<std::string>& exec_args) {
  FPrintF(
      stderr,
      "Error: Node.js must be built with the LIEF library to support built-in"
      " single executable applications.\n");
  return ExitCode::kGenericUserError;
}
#endif  // HAVE_LIEF

}  // namespace sea
}  // namespace node
