
#ifndef SRC_NODE_SNAPSHOTABLE_H_
#define SRC_NODE_SNAPSHOTABLE_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "v8.h"
namespace node {

v8::StartupData SerializeNodeContextInternalFields(v8::Local<v8::Object> holder,
                                                   int index,
                                                   void* env);
void DeserializeNodeInternalFields(v8::Local<v8::Object> holder,
                                   int index,
                                   v8::StartupData payload,
                                   void* env);
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_SNAPSHOTABLE_H_
