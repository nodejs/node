#include "node_sea.h"

#ifdef HAVE_LIEF
// Temporarily undefine DEBUG because LIEF uses it as an enum name.
#if defined(DEBUG)
#define SAVED_DEBUG_VALUE DEBUG
#undef DEBUG
#include "LIEF/LIEF.hpp"
#define DEBUG SAVED_DEBUG_VALUE
#undef SAVED_DEBUG_VALUE
#else  // defined(DEBUG)
#include "LIEF/LIEF.hpp"
#endif  // defined(DEBUG)
#endif  // HAVE_LIEF

#include "debug_utils-inl.h"
#include "env-inl.h"
#include "node_exit_code.h"
#include "simdutf.h"
#include "util-inl.h"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

// Split them into two for use later to avoid conflicting with postject.
#define SEA_SENTINEL_PREFIX "NODE_SEA_FUSE"
#define SEA_SENTINEL_TAIL "fce680ab2cc467b6e072b8b5df1996b2"

// The POSTJECT_SENTINEL_FUSE macro is a string of random characters selected by
// the Node.js project that is present only once in the entire binary. It is
// used by the postject_has_resource() function to efficiently detect if a
// resource has been injected. See
// https://github.com/nodejs/postject/blob/35343439cac8c488f2596d7c4c1dddfec1fddcae/postject-api.h#L42-L45.
#define POSTJECT_SENTINEL_FUSE SEA_SENTINEL_PREFIX "_" SEA_SENTINEL_TAIL
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
  std::string error_message;  // Populated when result != kSuccess
};

template <typename... Args>
static void DebugLog(const char* format, Args&&... args) {
  per_process::Debug(DebugCategory::SEA, "[SEA] ");
  per_process::Debug(DebugCategory::SEA, format, std::forward<Args>(args)...);
}

InjectOutput InjectIntoELF(const std::vector<uint8_t>& executable,
                           const std::string& note_name,
                           const std::vector<uint8_t>& data) {
  DebugLog("Parsing ELF binary for injection...\n");
  std::unique_ptr<LIEF::ELF::Binary> binary =
      LIEF::ELF::Parser::parse(executable);
  if (!binary) {
    return {InjectResult::kError, {}, "Failed to parse ELF binary"};
  }

  constexpr uint32_t kNoteType = 0;

  std::unique_ptr<LIEF::ELF::Note> existing_note;
  DebugLog("Searching for existing note \'%s\'\n", note_name);
  for (const auto& n : binary->notes()) {
    // LIEF can return a length longer than it actually is, so use compare here.
    if (n.name().compare(0, note_name.size(), note_name) == 0) {
      DebugLog("Found existing note %s\n", note_name);
      return {InjectResult::kAlreadyExists,
              {},
              SPrintF("note section %s already exists in the ELF executable",
                      note_name)};
    }
  }

  DebugLog("No existing note found. Proceeding to add new note.\n");

  auto new_note =
      LIEF::ELF::Note::create(note_name, kNoteType, data, kELFSectionName);
  if (!new_note) {
    return {InjectResult::kError,
            {},
            SPrintF("Failed to create new ELF note %s", note_name)};
  }
  binary->add(*new_note);

  LIEF::ELF::Builder::config_t cfg;
  cfg.notes = true;            // Ensure notes are rebuilt
  cfg.dynamic_section = true;  // Ensure PT_DYNAMIC is rebuilt

  DebugLog("Building modified ELF binary with new note...\n");
  LIEF::ELF::Builder builder(*binary, cfg);
  builder.build();
  if (builder.get_build().empty()) {
    return {InjectResult::kError, {}, "Failed to build modified ELF binary"};
  }
  return InjectOutput{InjectResult::kSuccess, builder.get_build(), ""};
}

InjectOutput InjectIntoMachO(const std::vector<uint8_t>& executable,
                             const std::string& segment_name,
                             const std::string& section_name,
                             const std::vector<uint8_t>& data) {
  DebugLog("Parsing Mach-O binary for injection...\n");
  std::unique_ptr<LIEF::MachO::FatBinary> fat_binary =
      LIEF::MachO::Parser::parse(executable);
  if (!fat_binary) {
    return {InjectResult::kError, {}, "Failed to parse Mach-O binary"};
  }

  // Inject into all Mach-O binaries if there's more than one in a fat binary
  DebugLog(
      "Searching for existing section %s/%s\n", segment_name, section_name);
  for (auto& binary : *fat_binary) {
    LIEF::MachO::SegmentCommand* segment = binary.get_segment(segment_name);
    if (!segment) {
      // Create the segment and mark it read-only
      LIEF::MachO::SegmentCommand new_segment(segment_name);
      // Use SegmentCommand::VM_PROTECTIONS enum values (READ)
      new_segment.max_protection(static_cast<uint32_t>(
          LIEF::MachO::SegmentCommand::VM_PROTECTIONS::READ));
      new_segment.init_protection(static_cast<uint32_t>(
          LIEF::MachO::SegmentCommand::VM_PROTECTIONS::READ));
      LIEF::MachO::Section section(section_name, data);
      new_segment.add_section(section);
      binary.add(new_segment);
    } else {
      // Check if the section exists
      LIEF::MachO::Section* existing_section =
          segment->get_section(section_name);
      if (existing_section) {
        // TODO(joyeecheung): support overwrite.
        return {InjectResult::kAlreadyExists,
                {},
                SPrintF("Segment/section %s/%s already exists in the Mach-O "
                        "executable",
                        segment_name,
                        section_name)};
      }
      LIEF::MachO::Section section(section_name, data);
      binary.add_section(*segment, section);
    }

    // It will need to be signed again anyway, so remove the signature
    if (binary.has_code_signature()) {
      DebugLog("Removing existing code signature\n");
      if (binary.remove_signature()) {
        DebugLog("Code signature removed successfully\n");
      } else {
        return {InjectResult::kError,
                {},
                "Failed to remove existing code signature"};
      }
    }
  }

  return InjectOutput{InjectResult::kSuccess, fat_binary->raw(), ""};
}

InjectOutput InjectIntoPE(const std::vector<uint8_t>& executable,
                          const std::string& resource_name,
                          const std::vector<uint8_t>& data) {
  DebugLog("Parsing PE binary for injection...\n");
  std::unique_ptr<LIEF::PE::Binary> binary =
      LIEF::PE::Parser::parse(executable);
  if (!binary) {
    return {InjectResult::kError, {}, "Failed to parse PE binary"};
  }

  // TODO(postject) - lief.PE.ResourcesManager doesn't support RCDATA it seems,
  // add support so this is simpler?
  if (!binary->has_resources()) {
    // TODO(postject) - Handle this edge case by creating the resource tree
    return {
        InjectResult::kError, {}, "PE binary has no resources, cannot inject"};
  }

  LIEF::PE::ResourceNode* resources = binary->resources();
  LIEF::PE::ResourceNode* rcdata_node = nullptr;
  LIEF::PE::ResourceNode* id_node = nullptr;

  // First level => Type (ResourceDirectory node)
  DebugLog("Locating/creating RCDATA resource node\n");
  constexpr uint32_t RCDATA_ID =
      static_cast<uint32_t>(LIEF::PE::ResourcesManager::TYPE::RCDATA);
  auto rcdata_node_iter = std::find_if(std::begin(resources->childs()),
                                       std::end(resources->childs()),
                                       [](const LIEF::PE::ResourceNode& node) {
                                         return node.id() == RCDATA_ID;
                                       });
  if (rcdata_node_iter != std::end(resources->childs())) {
    DebugLog("Found existing RCDATA resource node\n");
    rcdata_node = &*rcdata_node_iter;
  } else {
    DebugLog("Creating new RCDATA resource node\n");
    LIEF::PE::ResourceDirectory new_rcdata_node;
    new_rcdata_node.id(RCDATA_ID);
    rcdata_node = &resources->add_child(new_rcdata_node);
  }

  // Second level => ID (ResourceDirectory node)
  DebugLog("Locating/creating ID resource node for %s\n", resource_name);
  DCHECK(simdutf::validate_ascii(resource_name.data(), resource_name.size()));
  std::u16string resource_name_u16(resource_name.begin(), resource_name.end());
  auto id_node_iter =
      std::find_if(std::begin(rcdata_node->childs()),
                   std::end(rcdata_node->childs()),
                   [resource_name_u16](const LIEF::PE::ResourceNode& node) {
                     return node.name() == resource_name_u16;
                   });
  if (id_node_iter != std::end(rcdata_node->childs())) {
    DebugLog("Found existing ID resource node for %s\n", resource_name);
    id_node = &*id_node_iter;
  } else {
    // TODO(postject) - This isn't documented, but if this isn't set then
    // LIEF won't save the name. Seems like LIEF should be able
    // to automatically handle this if you've set the node's name
    DebugLog("Creating new ID resource node for %s\n", resource_name);
    LIEF::PE::ResourceDirectory new_id_node;
    new_id_node.name(resource_name);
    new_id_node.id(0x80000000);
    id_node = &rcdata_node->add_child(new_id_node);
  }

  // Third level => Lang (ResourceData node)
  DebugLog("Locating existing language resource node for %s\n", resource_name);
  if (id_node->childs() != std::end(id_node->childs())) {
    DebugLog("Found existing language resource node for %s\n", resource_name);
    return {InjectResult::kAlreadyExists,
            {},
            SPrintF("Resource %s already exists in the PE executable",
                    resource_name)};
  }

  LIEF::PE::ResourceData lang_node(data);
  id_node->add_child(lang_node);

  DebugLog("Rebuilding PE resources with new data for %s\n", resource_name);
  // Write out the binary, only modifying the resources
  LIEF::PE::Builder::config_t cfg;
  cfg.resources = true;
  cfg.rsrc_section = ".rsrc";  // ensure section name
  LIEF::PE::Builder builder(*binary, cfg);
  if (!builder.build()) {
    return {InjectResult::kError, {}, "Failed to build modified PE binary"};
  }

  return InjectOutput{InjectResult::kSuccess, builder.get_build(), ""};
}

void MarkSentinel(InjectOutput* output, const std::string& sentinel_fuse) {
  if (output == nullptr || output->result != InjectResult::kSuccess) return;
  std::string_view fuse(sentinel_fuse);
  std::string_view data_view(reinterpret_cast<char*>(output->data.data()),
                             output->data.size());

  size_t first_pos = data_view.find(fuse);
  DebugLog("Searching for fuse: %s\n", sentinel_fuse);
  if (first_pos == std::string::npos) {
    output->result = InjectResult::kError;
    output->error_message = SPrintF("sentinel %s not found", sentinel_fuse);
    return;
  }

  size_t last_pos = data_view.rfind(fuse);
  if (first_pos != last_pos) {
    output->result = InjectResult::kError;
    output->error_message =
        SPrintF("found more than one occurrence of sentinel %s", sentinel_fuse);
    return;
  }

  size_t colon_pos = first_pos + fuse.size();
  if (colon_pos >= data_view.size() || data_view[colon_pos] != ':') {
    output->result = InjectResult::kError;
    output->error_message =
        SPrintF("missing ':' after sentinel %s", sentinel_fuse);
    return;
  }

  size_t idx = colon_pos + 1;
  // Expecting ':0' or ':1' after the fuse
  if (idx >= data_view.size()) {
    output->result = InjectResult::kError;
    output->error_message = "Sentinel index out of range";
    return;
  }

  DebugLog("Found fuse: %s\n", data_view.substr(first_pos, fuse.size() + 2));

  if (data_view[idx] == '0') {
    DebugLog("Marking sentinel as 1\n");
    output->data.data()[idx] = '1';
  } else if (data_view[idx] == '1') {
    output->result = InjectResult::kAlreadyExists;
    output->error_message = "Sentinel is already marked";
    return;
  } else {
    output->result = InjectResult::kError;
    output->error_message = SPrintF("Sentinel has invalid value %d",
                                    static_cast<int>(data_view[idx]));
    return;
  }
  DebugLog("Processed fuse: %s\n",
           data_view.substr(first_pos, fuse.size() + 2));

  return;
}

InjectOutput InjectResource(const std::vector<uint8_t>& exe,
                            const std::string& resource_name,
                            const std::vector<uint8_t>& res,
                            const std::string& macho_segment_name) {
  if (LIEF::ELF::is_elf(exe)) {
    return InjectIntoELF(exe, resource_name, res);
  } else if (LIEF::MachO::is_macho(exe)) {
    std::string sec = resource_name;
    if (!(sec.rfind("__", 0) == 0)) sec = "__" + sec;
    return InjectIntoMachO(exe, macho_segment_name, sec, res);
  } else if (LIEF::PE::is_pe(exe)) {
    std::string upper_name = resource_name;
    // Convert resource name to uppercase as PE resource names are
    // case-insensitive.
    std::transform(upper_name.begin(),
                   upper_name.end(),
                   upper_name.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return InjectIntoPE(exe, upper_name, res);
  }

  return {InjectResult::kUnknownFormat,
          {},
          "Executable must be a supported format: ELF, PE, or Mach-O"};
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
    config.executable_path = Environment::GetExecPath(args);
  }

  // Get file permissions from source executable to copy over later.
  uv_fs_t req;
  int r = uv_fs_stat(nullptr, &req, config.executable_path.c_str(), nullptr);
  if (r != 0) {
    FPrintF(stderr,
            "Error: Couldn't stat executable %s: %s\n",
            config.executable_path,
            uv_strerror(r));
    uv_fs_req_cleanup(&req);
    return ExitCode::kGenericUserError;
  }
  int src_mode = static_cast<int>(req.statbuf.st_mode);
  uv_fs_req_cleanup(&req);

  std::vector<uint8_t> exe_data;
  r = ReadFileSync(config.executable_path.c_str(), &exe_data);
  if (r != 0) {
    FPrintF(stderr,
            "Error: Couldn't read executable %s: %s\n",
            config.executable_path,
            uv_strerror(r));
    return ExitCode::kGenericUserError;
  }

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
  std::string fuse = std::string(SEA_SENTINEL_PREFIX) + "_" + SEA_SENTINEL_TAIL;
  InjectOutput out = InjectResource(
      exe_data, kSEAResourceName, sea_blob_u8, kMachoSegmentName);
  if (out.result == InjectResult::kSuccess) {
    MarkSentinel(&out, fuse);
  }

  if (out.result != InjectResult::kSuccess) {
    if (!out.error_message.empty()) {
      FPrintF(stderr, "Error: %s\n", out.error_message);
    }
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
