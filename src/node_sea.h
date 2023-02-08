#ifndef SRC_NODE_SEA_H_
#define SRC_NODE_SEA_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#if !defined(DISABLE_SINGLE_EXECUTABLE_APPLICATION)

#include "v8-local-handle.h"
#include "v8-value.h"

#include <tuple>

namespace node {

class Environment;

namespace per_process {
namespace sea {

bool IsSingleExecutable();
v8::MaybeLocal<v8::Value> StartSingleExecutableExecution(Environment* env);
std::tuple<int, char**> FixupArgsForSEA(int argc, char** argv);

}  // namespace sea
}  // namespace per_process
}  // namespace node

#endif  // !defined(DISABLE_SINGLE_EXECUTABLE_APPLICATION)

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_SEA_H_
