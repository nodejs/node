#ifndef SRC_NODE_SINGLE_BINARY_H_
#define SRC_NODE_SINGLE_BINARY_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

namespace node {
namespace single_binary {

struct NewArgs {
  bool singleBinary;
  int argc;
  char** argv;
};

NewArgs* checkForSingleBinary(int argc, char** argv);

}  // namespace single_binary
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_SINGLE_BINARY_H_
