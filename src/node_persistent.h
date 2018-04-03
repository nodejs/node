#ifndef SRC_NODE_PERSISTENT_H_
#define SRC_NODE_PERSISTENT_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "v8.h"

namespace node {

template <typename T>
struct ResetInDestructorPersistentTraits {
  static const bool kResetInDestructor = true;
  template <typename S, typename M>
  // Disallow copy semantics by leaving this unimplemented.
  inline static void Copy(
      const v8::Persistent<S, M>&,
      v8::Persistent<T, ResetInDestructorPersistentTraits<T>>*);
};

// v8::Persistent does not reset the object slot in its destructor.  That is
// acknowledged as a flaw in the V8 API and expected to change in the future
// but for now node::Persistent is the easier and safer alternative.
template <typename T>
using Persistent = v8::Persistent<T, ResetInDestructorPersistentTraits<T>>;

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_PERSISTENT_H_
