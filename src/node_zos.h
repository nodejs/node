#ifndef SRC_NODE_ZOS_H_
#define SRC_NODE_ZOS_H_

#include "node_exit_code.h"

#include <csignal>

namespace node {

// used by node.cc:
ExitCode zosStart();
void zosCreateSignalHandler();
void zosCancelSignalHandler();
void zosSignalExit(int signo, siginfo_t* info, void* ucontext);
void zosDestroyThread();
// used by inspector_agent.cc:
void zosSetThreadAltStack();

}  // namespace node
#endif  // SRC_NODE_ZOS_H_
