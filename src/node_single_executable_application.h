#ifndef SRC_NODE_SINGLE_EXECUTABLE_APPLICATION_H_
#define SRC_NODE_SINGLE_EXECUTABLE_APPLICATION_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

namespace node {
namespace single_executable_application {

struct single_executable_replacement_args {
  bool single_executable_application;
  int argc;
  char** argv;
};

single_executable_replacement_args* CheckForSingleBinary(int argc, char** argv);

}  // namespace single_executable_application
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_SINGLE_EXECUTABLE_APPLICATION_H_
