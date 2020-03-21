#include "snapshot_support.h"  // NOLINT(build/include_inline)
#include "snapshot_support-inl.h"
#include "debug_utils-inl.h"
#include "env-inl.h"
#include "json_utils.h"  // EscapeJsonChars
#include "util.h"
#include <sstream>
#include <iomanip>  // std::setw

using v8::Context;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::Nothing;
using v8::Object;

namespace node {

Snapshottable::~Snapshottable() {}

void Snapshottable::Serialize(SnapshotCreateData* snapshot_data) const {
  snapshot_data->add_error("Unserializable object encountered");
}

#define SNAPSHOT_TAGS(V)                     \
  V(kStartEntry)                             \
  V(kEndEntry)                               \
  V(kBool)                                   \
  V(kInt32)                                  \
  V(kInt64)                                  \
  V(kUint32)                                 \
  V(kUint64)                                 \
  V(kIndex)                                  \
  V(kString)                                 \
  V(kContextIndependentObject)               \
  V(kObject)                                 \

enum Tag {
#define V(name) name,
  SNAPSHOT_TAGS(V)
#undef V
};

static std::string TagName(int tag) {
#define V(name) if (tag == name) return #name;
  SNAPSHOT_TAGS(V)
#undef V
  return SPrintF("(unknown tag %d)", tag);
}

const uint8_t SnapshotDataBase::kContextIndependentObjectTag =
  kContextIndependentObject;
const uint8_t SnapshotDataBase::kObjectTag = kObject;

SnapshotDataBase::SaveStateScope::SaveStateScope(
    SnapshotDataBase* snapshot_data)
  : snapshot_data_(snapshot_data), state_(snapshot_data->state_) {
}

SnapshotDataBase::SaveStateScope::~SaveStateScope() {
  snapshot_data_->state_ = std::move(state_);
}


bool SnapshotDataBase::HasSpace(size_t length) const {
  return storage_.size() - state_.current_index >= length;
}

void SnapshotCreateData::WriteRawData(const uint8_t* data, size_t length) {
  storage_.resize(storage_.size() + length);
  memcpy(storage_.data() + state_.current_index, data, length);
  state_.current_index += length;
  CHECK_EQ(state_.current_index, storage_.size());
}

bool SnapshotReadData::ReadRawData(uint8_t* data, size_t length) {
  if (UNLIKELY(!HasSpace(length))) {
    add_error("Unexpected end of snapshot data input");
    return false;
  }
  memcpy(data, storage_.data() + state_.current_index, length);
  state_.current_index += length;
  return true;
}

void SnapshotCreateData::WriteTag(uint8_t tag) {
  WriteRawData(&tag, 1);
}

bool SnapshotReadData::ReadTag(uint8_t expected) {
  uint8_t actual;
  if (!ReadRawData(&actual, 1)) return false;
  if (actual != expected) {
    add_error(SPrintF("Tried to read object of type %s, found type %s instead",
                      TagName(expected), TagName(actual)));
    return false;
  }
  return true;
}

Maybe<uint8_t> SnapshotReadData::PeekTag() {
  SaveStateScope state_scope(this);
  uint8_t tag;
  if (!ReadRawData(&tag, 1)) return Nothing<uint8_t>();
  return Just(tag);
}

void SnapshotCreateData::StartWriteEntry(const char* name) {
  WriteTag(kStartEntry);
  WriteString(name);
  state_.entry_stack.push_back(name);
}

void SnapshotCreateData::EndWriteEntry() {
  if (state_.entry_stack.empty()) {
    add_error("Attempting to end entry on empty stack, "
              "more EndWriteEntry() than StartWriteEntry() calls");
    return;
  }

  state_.entry_stack.pop_back();
  WriteTag(kEndEntry);
}

void SnapshotCreateData::WriteBool(bool value) {
  WriteTag(kBool);
  uint8_t data = value ? 1 : 0;
  WriteRawData(&data, 1);
}

void SnapshotCreateData::WriteInt32(int32_t value) {
  WriteTag(kInt32);
  WriteRawData(reinterpret_cast<const uint8_t*>(&value), sizeof(value));
}

void SnapshotCreateData::WriteInt64(int64_t value) {
  WriteTag(kInt64);
  WriteRawData(reinterpret_cast<const uint8_t*>(&value), sizeof(value));
}

void SnapshotCreateData::WriteUint32(uint32_t value) {
  WriteTag(kUint32);
  WriteRawData(reinterpret_cast<const uint8_t*>(&value), sizeof(value));
}

void SnapshotCreateData::WriteUint64(uint64_t value) {
  WriteTag(kUint64);
  WriteRawData(reinterpret_cast<const uint8_t*>(&value), sizeof(value));
}

void SnapshotCreateData::WriteIndex(V8SnapshotIndex value) {
  WriteTag(kIndex);
  WriteRawData(reinterpret_cast<const uint8_t*>(&value), sizeof(value));
}

void SnapshotCreateData::WriteString(const char* str, size_t length) {
  WriteTag(kString);
  if (length == static_cast<size_t>(-1)) length = strlen(str);
  WriteUint64(length);
  WriteRawData(reinterpret_cast<const uint8_t*>(str), length);
}

void SnapshotCreateData::WriteString(const std::string& str) {
  WriteString(str.c_str(), str.size());
}

v8::Maybe<std::string> SnapshotReadData::StartReadEntry(const char* expected) {
  if (!ReadTag(kStartEntry)) return Nothing<std::string>();
  std::string actual;
  if (!ReadString().To(&actual)) return Nothing<std::string>();
  if (expected != nullptr && actual != expected) {
    add_error(SPrintF("Tried to read start of entry %s, found entry %s",
                      expected, actual));
    return Nothing<std::string>();
  }
  state_.entry_stack.push_back(actual);
  return Just(std::move(actual));
}

v8::Maybe<bool> SnapshotReadData::EndReadEntry() {
  if (!ReadTag(kEndEntry)) return Nothing<bool>();
  if (state_.entry_stack.empty()) {
    add_error("Attempting to end entry on empty stack, "
              "more EndReadEntry() than StartReadEntry() calls");
    return Nothing<bool>();
  }
  state_.entry_stack.pop_back();
  return Just(true);
}

v8::Maybe<bool> SnapshotReadData::ReadBool() {
  if (!ReadTag(kBool)) return Nothing<bool>();
  uint8_t value;
  if (!ReadRawData(&value, 1)) return Nothing<bool>();
  return Just(static_cast<bool>(value));
}

v8::Maybe<int32_t> SnapshotReadData::ReadInt32() {
  if (!ReadTag(kInt32)) return Nothing<int32_t>();
  int32_t value;
  if (!ReadRawData(reinterpret_cast<uint8_t*>(&value), sizeof(value)))
    return Nothing<int32_t>();
  return Just(value);
}

v8::Maybe<int64_t> SnapshotReadData::ReadInt64() {
  if (!ReadTag(kInt64)) return Nothing<int64_t>();
  int64_t value;
  if (!ReadRawData(reinterpret_cast<uint8_t*>(&value), sizeof(value)))
    return Nothing<int64_t>();
  return Just(value);
}

v8::Maybe<uint32_t> SnapshotReadData::ReadUint32() {
  if (!ReadTag(kUint32)) return Nothing<uint32_t>();
  uint32_t value;
  if (!ReadRawData(reinterpret_cast<uint8_t*>(&value), sizeof(value)))
    return Nothing<uint32_t>();
  return Just(static_cast<uint32_t>(value));
}

v8::Maybe<uint64_t> SnapshotReadData::ReadUint64() {
  if (!ReadTag(kUint64)) return Nothing<uint64_t>();
  uint64_t value;
  if (!ReadRawData(reinterpret_cast<uint8_t*>(&value), sizeof(value)))
    return Nothing<uint64_t>();
  return Just(value);
}

v8::Maybe<V8SnapshotIndex> SnapshotReadData::ReadIndex() {
  if (!ReadTag(kIndex)) return Nothing<size_t>();
  size_t value;
  if (!ReadRawData(reinterpret_cast<uint8_t*>(&value), sizeof(value)))
    return Nothing<size_t>();
  return Just(value);
}

v8::Maybe<std::string> SnapshotReadData::ReadString() {
  if (!ReadTag(kString)) return Nothing<std::string>();
  uint64_t size;
  if (!ReadUint64().To(&size)) return Nothing<std::string>();
  std::string str(size, '\0');
  if (!ReadRawData(reinterpret_cast<uint8_t*>(&str[0]), size))
    return Nothing<std::string>();
  return Just(std::move(str));
}

v8::Maybe<bool> SnapshotReadData::Finish() {
  if (!state_.entry_stack.empty()) {
    add_error("Entries left on snapshot stack, EndReadEntry() missing");
    return Nothing<bool>();
  }

  if (state_.current_index != storage_.size()) {
    add_error("Unexpected data past the end of the snapshot data");
    return Nothing<bool>();
  }

  storage_.clear();
  storage_.shrink_to_fit();
  state_ = State{};
  return Just(true);
}

void SnapshotDataBase::add_error(const std::string& error) {
  std::string message = "At [";
  message += std::to_string(state_.current_index);
  message += "] ";
  for (const std::string& entry : state_.entry_stack) {
    message += entry;
    message += ':';
  }
  message += " ";
  message += error;
  state_.errors.emplace_back(
      Error { state_.current_index, std::move(message) });
}

std::string SnapshotReadData::DumpLine::ToString() const {
  std::ostringstream os;
  os << std::setw(6) << index << ' ';
  for (size_t i = 0; i < depth; i++) os << "  ";
  os << description;
  return os.str();
}

void SnapshotDataBase::DumpToStderr() {
  std::vector<SnapshotReadData::DumpLine> lines;
  std::vector<Error> errors;
  std::tie(lines, errors) = SnapshotReadData(storage()).Dump();

  for (const SnapshotReadData::DumpLine& line : lines)
    fprintf(stderr, "%s\n", line.ToString().c_str());
  if (!errors.empty()) {
    fprintf(stderr, "Encountered %zu snapshot errors:\n", errors.size());
    for (const Error& error : errors)
      fprintf(stderr, "%s\n", error.message.c_str());
  }
}

void SnapshotDataBase::PrintErrorsAndAbortIfAny() {
  if (errors().empty()) return;

  std::vector<SnapshotReadData::DumpLine> lines;
  std::tie(lines, std::ignore) = SnapshotReadData(storage()).Dump();

  fprintf(stderr, "Encountered %zu snapshot errors:\n", errors().size());
  for (const Error& error : errors()) {
    fprintf(stderr, "%s\nAround:\n", error.message.c_str());
    size_t i;
    for (i = 0; i < lines.size(); i++) {
      if (lines[i].index >= error.index) break;
    }
    size_t start_print_range = std::max<int64_t>(i - 5, 0);
    size_t end_print_range = std::min<int64_t>(i + 5, lines.size() - 1);
    for (size_t j = start_print_range; j <= end_print_range; j++) {
      fprintf(stderr,
              "%c %s\n",
              i == j + 1 ? '*' : ' ',  // Mark the line presumed to be at fault.
              lines[j].ToString().c_str());
    }
  }
  fprintf(stderr,
          "(`node --dump-snapshot` dumps the full snapshot data contained "
          "in a node binary)\n");
  fflush(stderr);
  Abort();
}

std::pair<std::vector<SnapshotReadData::DumpLine>,
          std::vector<SnapshotDataBase::Error>> SnapshotReadData::Dump() {
  SaveStateScope state_scope(this);
  state_ = State{};

  std::vector<DumpLine> ret;

  while (state_.current_index < storage_.size() && errors().empty()) {
    uint8_t tag;
    if (!PeekTag().To(&tag)) {
      ReadTag(0);  // PeekTag() failing means EOF, this re-generates that error.
      break;
    }

    DumpLine line = { state_.current_index, state_.entry_stack.size(), "" };

    switch (tag) {
      case kStartEntry: {
        std::string str;
        if (!StartReadEntry(nullptr).To(&str)) break;
        line.description = SPrintF("StartEntry: [%s]", str);
        break;
      }
      case kEndEntry: {
        if (EndReadEntry().IsNothing()) break;
        line.description = "EndEntry";
        break;
      }
      case kBool: {
        bool value;
        if (!ReadBool().To(&value)) break;
        line.description = SPrintF("Bool: %s", value);
        break;
      }
      case kInt32: {
        int32_t value;
        if (!ReadInt32().To(&value)) break;
        line.description = SPrintF("Int32: %s", value);
        break;
      }
      case kInt64: {
        int64_t value;
        if (!ReadInt64().To(&value)) break;
        line.description = SPrintF("Int64: %s", value);
        break;
      }
      case kUint32: {
        uint32_t value;
        if (!ReadUint32().To(&value)) break;
        line.description = SPrintF("Uint32: %s", value);
        break;
      }
      case kUint64: {
        uint64_t value;
        if (!ReadUint64().To(&value)) break;
        line.description = SPrintF("Uint64: %s", value);
        break;
      }
      case kString: {
        std::string value;
        if (!ReadString().To(&value)) break;
        if (value.size() > 120) {
          line.description = SPrintF("String: '%s' ... '%s'",
              EscapeJsonChars(value.substr(0, 90)),
              EscapeJsonChars(value.substr(value.size() - 20)));
        } else {
          line.description = SPrintF("String: '%s'", EscapeJsonChars(value));
        }
        break;
      }
      case kContextIndependentObjectTag:
      case kObjectTag:
        CHECK(ReadTag(tag));
        // fall-through
      case kIndex: {
        V8SnapshotIndex index;
        if (!ReadIndex().To(&index)) break;
        const char* type = tag == kContextIndependentObjectTag ?
          "Context-independent object index" :
          "Object index";
        if (index == kEmptyIndex)
          line.description = SPrintF("%s: (empty)", type);
        else
          line.description = SPrintF("%s: %s", type, index);
        break;
      }
      default: {
        // This will indicate that this is an unknown tag.
        line.description = TagName(tag);
        add_error(SPrintF("Encountered unknown type tag %d", tag));
        break;
      }
    }

    if (!line.description.empty())
      ret.emplace_back(std::move(line));
  }

  return std::make_pair(ret, errors());
}


Maybe<BaseObject*> SnapshotReadData::GetBaseObjectFromV8Object(
    Local<Context> context, Local<Object> obj) {
  Environment* env = Environment::GetCurrent(context);
  if (!BaseObject::GetConstructorTemplate(env)->HasInstance(obj)) {
    add_error("Deserialized object is not a BaseObject");
    return v8::Nothing<BaseObject*>();
  }
  BaseObject* base_obj = BaseObject::FromJSObject(obj);
  if (base_obj == nullptr) {
    add_error("Deserialized BaseObject is empty");
    return v8::Nothing<BaseObject*>();
  }
  return Just(base_obj);
}

void ExternalReferences::AddPointer(intptr_t ptr) {
  DCHECK_NE(ptr, kEnd);
  references_.push_back(ptr);
}

std::map<std::string, ExternalReferences*>* ExternalReferences::map() {
  static std::map<std::string, ExternalReferences*> map_;
  return &map_;
}

std::vector<intptr_t> ExternalReferences::get_list() {
  static std::vector<intptr_t> list;
  if (list.empty()) {
    for (const auto& entry : *map()) {
      std::vector<intptr_t>* source = &entry.second->references_;
      list.insert(list.end(), source->begin(), source->end());
      source->clear();
      source->shrink_to_fit();
    }
  }
  return list;
}

void ExternalReferences::Register(const char* id, ExternalReferences* self) {
  auto result = map()->insert({id, this});
  CHECK(result.second);
}

const intptr_t ExternalReferences::kEnd = reinterpret_cast<intptr_t>(nullptr);

BaseObjectDeserializer::BaseObjectDeserializer(
    const std::string& name, Callback callback) {
  auto result = map()->insert({name, callback});
  CHECK(result.second);
}

BaseObject* BaseObjectDeserializer::Deserialize(
    const std::string& name,
    Environment* env,
    SnapshotReadData* snapshot_data) {
  Callback callback = (*map())[name];
  if (callback == nullptr) {
    snapshot_data->add_error(SPrintF("Unknown BaseObject type %s", name));
    return nullptr;
  }
  return callback(env, snapshot_data);
}

std::map<std::string, BaseObjectDeserializer::Callback>*
BaseObjectDeserializer::map() {
  static std::map<std::string, Callback> map_;
  return &map_;
}

}  // namespace node
