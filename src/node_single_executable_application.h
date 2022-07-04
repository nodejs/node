#ifndef SRC_NODE_SINGLE_EXECUTABLE_APPLICATION_H_
#define SRC_NODE_SINGLE_EXECUTABLE_APPLICATION_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <string>
#include <vector>

namespace node {
namespace single_executable_application {

bool CheckForSingleBinary(int argc,
                          char** argv,
                          std::vector<std::string>* new_argv);

}  // namespace single_executable_application
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_SINGLE_EXECUTABLE_APPLICATION_H_
