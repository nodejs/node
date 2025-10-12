#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#ifdef DEBUG
#include "util.h"
#endif  // DEBUG

namespace node {
namespace debug {

#ifdef DEBUG
void TrackV8FastApiCall(FastStringKey key);
int GetV8FastApiCallCount(FastStringKey key);

#define TRACK_V8_FAST_API_CALL(key)                                            \
  node::debug::TrackV8FastApiCall(FastStringKey(key))
#else  // !DEBUG
#define TRACK_V8_FAST_API_CALL(key)
#endif  // DEBUG

}  // namespace debug
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
