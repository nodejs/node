#ifndef SRC_SNAPSHOT_SUPPORT_H_
#define SRC_SNAPSHOT_SUPPORT_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "v8.h"
#include <map>

namespace node {

class ExternalReferences {
 public:
  // Create static instances of this class to register a list of external
  // references for usage in snapshotting. Usually, this includes all C++
  // binding functions. `id` can be any string, as long as it is unique
  // (e.g. the current file name as retrieved by __FILE__).
  template <typename... Args>
  inline ExternalReferences(const char* id, Args*... args);

  // Returns the list of all references collected so far, not yet terminated
  // by kEnd.
  static std::vector<intptr_t> get_list();

  static const intptr_t kEnd;  // The end-of-list marker used by V8, nullptr.

 private:
  void Register(const char* id, ExternalReferences* self);
  static std::map<std::string, ExternalReferences*>* map();
  std::vector<intptr_t> references_;

  void AddPointer(intptr_t ptr);
  inline void HandleArgs();
  template <typename T, typename... Args>
  inline void HandleArgs(T* ptr, Args*... args);
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_SNAPSHOT_SUPPORT_H_
