#ifndef SRC_NODE_MESSAGES_SRC_H_
#define SRC_NODE_MESSAGES_SRC_H_

#define NODE_MESSAGE_UNKNOWN "(Message Unknown)"

#define NODE_MESSAGES(XX)                                                     \
  XX(NODE_ASSERTION_ERROR, "assertion error")                                 \
  XX(NODE_HRTIME_ARRAY, "process.hrtime() only accepts an Array tuple")       \
  XX(NODE_FUNCTION_REQUIRED, "'{0}' must be a function")                      \
  XX(NODE_UNKNOWN_STREAM_FILE_TYPE, "Unknown stream file type!")              \
  XX(NODE_UNKNOWN_STDIN_FILE_TYPE, "Unknown stdin file type!")                \
  XX(NODE_CLOSE_STDOUT, "process.stdout cannot be closed.")                   \
  XX(NODE_CLOSE_STDERR, "process.stderr cannot be closed.")                   \
  XX(NODE_INVALID_PID, "invalid pid")                                         \
  XX(NODE_UNKNOWN_SIGNAL, "Unknown signal: {0}")                              \
  XX(NODE_UNKNOWN_NATIVE_MODULE, "No such native module")                     \
  XX(NODE_UNEXPECTED, "unexpected {0}")                                       \

#endif  // SRC_NODE_MESSAGES_SRC_H_
