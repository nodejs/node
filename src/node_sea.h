#ifndef SRC_NODE_SEA_H_
#define SRC_NODE_SEA_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#if !defined(DISABLE_SINGLE_EXECUTABLE_APPLICATION)

#include <cinttypes>
#include <tuple>

namespace node {
namespace sea {

bool IsSingleExecutable();
std::tuple<int, char**> FixupArgsForSEA(int argc, char** argv);

}  // namespace sea
}  // namespace node

#endif  // !defined(DISABLE_SINGLE_EXECUTABLE_APPLICATION)

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_SEA_H_
