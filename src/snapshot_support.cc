#include "snapshot_support.h"  // NOLINT(build/include_inline)
#include "snapshot_support-inl.h"
#include "util.h"

namespace node {

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

}  // namespace node
