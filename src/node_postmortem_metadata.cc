// Need to import standard headers before redefining private, otherwise it
// won't compile.
#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <exception>
#include <forward_list>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iosfwd>
#include <iostream>
#include <istream>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <new>
#include <ostream>
#include <queue>
#include <set>
#include <sstream>
#include <stack>
#include <streambuf>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace node {
// Forward declaration needed before redefining private.
int GenDebugSymbols();
}  // namespace node


#define private friend int GenDebugSymbols(); private

#include "env.h"
#include "base_object-inl.h"
#include "handle_wrap.h"
#include "util-inl.h"
#include "req_wrap.h"
#include "v8abbr.h"

#define NODEDBG_SYMBOL(Name)  nodedbg_ ## Name

// nodedbg_offset_CLASS__MEMBER__TYPE: Describes the offset to a class member.
#define NODEDBG_OFFSET(Class, Member, Type) \
    NODEDBG_SYMBOL(offset_ ## Class ## __ ## Member ## __ ## Type)

// These are the constants describing Node internal structures. Every constant
// should use the format described above.  These constants are declared as
// global integers so that they'll be present in the generated node binary. They
// also need to be declared outside any namespace to avoid C++ name-mangling.
#define NODE_OFFSET_POSTMORTEM_METADATA(V)                                    \
  V(BaseObject, persistent_handle_, v8_Persistent_v8_Object,                  \
    BaseObject::persistent_handle_)                                           \
  V(Environment, handle_wrap_queue_, Environment_HandleWrapQueue,             \
    Environment::handle_wrap_queue_)                                          \
  V(Environment, req_wrap_queue_, Environment_ReqWrapQueue,                   \
    Environment::req_wrap_queue_)                                             \
  V(HandleWrap, handle_wrap_queue_, ListNode_HandleWrap,                      \
    HandleWrap::handle_wrap_queue_)                                           \
  V(Environment_HandleWrapQueue, head_, ListNode_HandleWrap,                  \
    Environment::HandleWrapQueue::head_)                                      \
  V(ListNode_HandleWrap, next_, uintptr_t, ListNode<HandleWrap>::next_)       \
  V(ReqWrap, req_wrap_queue_, ListNode_ReqWrapQueue,                          \
    ReqWrap<uv_req_t>::req_wrap_queue_)                                       \
  V(Environment_ReqWrapQueue, head_, ListNode_ReqWrapQueue,                   \
    Environment::ReqWrapQueue::head_)                                         \
  V(ListNode_ReqWrap, next_, uintptr_t, ListNode<ReqWrap<uv_req_t>>::next_)

extern "C" {
int nodedbg_const_Environment__kContextEmbedderDataIndex__int;
uintptr_t nodedbg_offset_ExternalString__data__uintptr_t;

#define V(Class, Member, Type, Accessor)                                      \
  NODE_EXTERN uintptr_t NODEDBG_OFFSET(Class, Member, Type);
  NODE_OFFSET_POSTMORTEM_METADATA(V)
#undef V
}

namespace node {

int GenDebugSymbols() {
  nodedbg_const_Environment__kContextEmbedderDataIndex__int =
      Environment::kContextEmbedderDataIndex;

  nodedbg_offset_ExternalString__data__uintptr_t = NODE_OFF_EXTSTR_DATA;

  #define V(Class, Member, Type, Accessor)                                    \
    NODEDBG_OFFSET(Class, Member, Type) = OffsetOf(&Accessor);
    NODE_OFFSET_POSTMORTEM_METADATA(V)
  #undef V

  return 1;
}

int debug_symbols_generated = GenDebugSymbols();

}  // namespace node
