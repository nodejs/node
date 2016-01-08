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
  XX(NODE_DEBUGAGENT_LISTENING, "Debugger listening on port {0}")             \
  XX(NODE_DEBUGAGENT_NO_BINDINGS, "Debugger agent running without bindings!") \
  XX(NODE_DEBUGAGENT_EXPECTED_HEADER,                                         \
    "Expected header, but failed to parse it")                                \
  XX(NODE_DEBUGAGENT_EXPECTED_CONTENT_LENGTH, "Expected content-length")      \
  XX(NODE_DEBUGGER_USAGE, "Usage")                                            \
  XX(NODE_DEBUGGER_ERROR,                                                     \
     "There was an internal error in the Node.js debugger. "                  \
     "Please report this error.")                                             \
  XX(NODE_DEBUGGER_UNKNOWN_STATE, "Unknown state")                            \
  XX(NODE_DEBUGGER_PROBLEM_REQLOOKUP, "problem with reqLookup")               \
  XX(NODE_DEBUGGER_NO_FRAMES, "No frames")                                    \
  XX(NODE_DEBUGGER_COMMANDS, "Commands")                                      \
  XX(NODE_DEBUGGER_BREAK_IN, "break in")                                      \
  XX(NODE_DEBUGGER_EXCEPTION_IN, "exception in")                              \
  XX(NODE_DEBUGGER_APP_NOT_RUNNING,                                           \
    "The application is not running. Try `run` instead")                      \
  XX(NODE_DEBUGGER_APP_RUNNING,                                               \
    "The application is already running. Try `restart` instead")              \
  XX(NODE_DEBUGGER_CANNOT_LIST_SOURCE,                                        \
    "Source code cannot be listed right now'")                                \
  XX(NODE_DEBUGGER_CANNOT_REQUEST_BACKTRACE,                                  \
    "Backtrace cannot be requested right now")                                \
  XX(NODE_DEBUGGER_EMPTY_STACK, "(empty stack)")                              \
  XX(NODE_DEBUGGER_WATCHERS, "Watchers")                                      \
  XX(NODE_DEBUGGER_CANNOT_DETERMINE_SCRIPT,                                   \
    "Cannot determine the current script, "                                   \
    "make sure the debugged process is paused.")                              \
  XX(NODE_DEBUGGER_SCRIPT_NAME_AMBIGUOUS, "Script name is ambiguous")         \
  XX(NODE_DEBUGGER_LINE_POSITIVE, "Line must be a positive value")            \
  XX(NODE_DEBUGGER_SCRIPT_NOT_LOADED,                                         \
    "Warning: script '{0}' was not loaded yet.'")                             \
  XX(NODE_DEBUGGER_SCRIPT_NOT_FOUND, "Script '{0}' not found")                \
  XX(NODE_DEBUGGER_BREAKPOINT_NOT_FOUND, "Breakpoint not found on line {0}")  \
  XX(NODE_DEBUGGER_REPL_EXIT, "Press Ctrl + C to leave debug repl")           \
  XX(NODE_DEBUGGER_TARGET_PROCESS, "Target process {0} does not exist.")      \
  XX(NODE_DEBUGGER_READY, "ok")                                               \
  XX(NODE_DEBUGGER_RESTORING_BREAKPOINT, "Restoring breakpoint")              \
  XX(NODE_DEBUGGER_PROGRAM_TERMINATED, "program terminated")                  \
  XX(NODE_DEBUGGER_UNHANDLED_RESPONSE, "unhandled res")                       \
  XX(NODE_DEBUGGER_CONNECTION_FAILED, "failed to connect, please retry")      \
  XX(NODE_DEBUGGER_CONNECTING, "connecting to")                               \

#endif  // SRC_NODE_MESSAGES_SRC_H_
