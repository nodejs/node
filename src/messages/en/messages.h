#ifndef SRC_NODE_MESSAGES_SRC_H_
#define SRC_NODE_MESSAGES_SRC_H_

#define NODE_MESSAGE_UNKNOWN "(Message Unknown)"

#define NODE_DEPRECATE_MESSAGE(what, alternate)                               \
  what " is deprecated. Use " alternate " instead."

#define NODE_MESSAGES(XX)                                                     \
  XX(ASSERTION_ERROR, "assertion error")                                      \
  XX(HRTIME_ARRAY, "process.hrtime() only accepts an Array tuple")            \
  XX(FUNCTION_REQUIRED, "'{0}' must be a function")                           \
  XX(UNKNOWN_STREAM_FILE_TYPE, "Unknown stream file type!")                   \
  XX(UNKNOWN_STDIN_FILE_TYPE, "Unknown stdin file type!")                     \
  XX(CLOSE_STDOUT, "process.stdout cannot be closed.")                        \
  XX(CLOSE_STDERR, "process.stderr cannot be closed.")                        \
  XX(INVALID_PID, "invalid pid")                                              \
  XX(UNKNOWN_SIGNAL, "Unknown signal: {0}")                                   \
  XX(UNKNOWN_NATIVE_MODULE, "No such native module")                          \
  XX(UNEXPECTED, "unexpected {0}")                                            \
  XX(DEBUGAGENT_LISTENING, "Debugger listening on port {0}")                  \
  XX(DEBUGAGENT_NO_BINDINGS, "Debugger agent running without bindings!")      \
  XX(DEBUGAGENT_EXPECTED_HEADER,                                              \
    "Expected header, but failed to parse it")                                \
  XX(DEBUGAGENT_EXPECTED_CONTENT_LENGTH, "Expected content-length")           \
  XX(DEBUGGER_USAGE, "Usage")                                                 \
  XX(DEBUGGER_ERROR,                                                          \
     "There was an internal error in the Node.js debugger. "                  \
     "Please report this error.")                                             \
  XX(DEBUGGER_UNKNOWN_STATE, "Unknown state")                                 \
  XX(DEBUGGER_PROBLEM_REQLOOKUP, "problem with reqLookup")                    \
  XX(DEBUGGER_NO_FRAMES, "No frames")                                         \
  XX(DEBUGGER_COMMANDS, "Commands")                                           \
  XX(DEBUGGER_BREAK_IN, "break in")                                           \
  XX(DEBUGGER_EXCEPTION_IN, "exception in")                                   \
  XX(DEBUGGER_APP_NOT_RUNNING,                                                \
    "The application is not running. Try `run` instead")                      \
  XX(DEBUGGER_APP_RUNNING,                                                    \
    "The application is already running. Try `restart` instead")              \
  XX(DEBUGGER_CANNOT_LIST_SOURCE,                                             \
    "Source code cannot be listed right now'")                                \
  XX(DEBUGGER_CANNOT_REQUEST_BACKTRACE,                                       \
    "Backtrace cannot be requested right now")                                \
  XX(DEBUGGER_EMPTY_STACK, "(empty stack)")                                   \
  XX(DEBUGGER_WATCHERS, "Watchers")                                           \
  XX(DEBUGGER_CANNOT_DETERMINE_SCRIPT,                                        \
    "Cannot determine the current script, "                                   \
    "make sure the debugged process is paused.")                              \
  XX(DEBUGGER_SCRIPT_NAME_AMBIGUOUS, "Script name is ambiguous")              \
  XX(DEBUGGER_LINE_POSITIVE, "Line must be a positive value")                 \
  XX(DEBUGGER_SCRIPT_NOT_LOADED,                                              \
    "Warning: script '{0}' was not loaded yet.'")                             \
  XX(DEBUGGER_SCRIPT_NOT_FOUND, "Script '{0}' not found")                     \
  XX(DEBUGGER_BREAKPOINT_NOT_FOUND, "Breakpoint not found on line {0}")       \
  XX(DEBUGGER_REPL_EXIT, "Press Ctrl + C to leave debug repl")                \
  XX(DEBUGGER_TARGET_PROCESS, "Target process {0} does not exist.")           \
  XX(DEBUGGER_READY, "ok")                                                    \
  XX(DEBUGGER_RESTORING_BREAKPOINT, "Restoring breakpoint")                   \
  XX(DEBUGGER_PROGRAM_TERMINATED, "program terminated")                       \
  XX(DEBUGGER_UNHANDLED_RESPONSE, "unhandled res")                            \
  XX(DEBUGGER_CONNECTION_FAILED, "failed to connect, please retry")           \
  XX(DEBUGGER_CONNECTING, "connecting to")                                    \
  XX(HTTP_AGENT_DEBUG_FREE_SOCKET, "have free socket")                        \
  XX(HTTP_AGENT_DEBUG_CALL_ONSOCKET, "call onSocket")                         \
  XX(HTTP_AGENT_DEBUG_WAIT_FOR_SOCKET, "wait for socket")                     \
  XX(HTTP_AGENT_DEBUG_SOCKETS, "sockets")                                     \
  XX(HTTP_AGENT_DEBUG_CLIENT_CLOSE, "CLIENT socket onClose")                  \
  XX(HTTP_AGENT_DEBUG_CLIENT_REMOVE, "CLIENT socket onRemove")                \
  XX(HTTP_AGENT_DEBUG_DESTROYED, "destroyed")                                 \
  XX(HTTP_AGENT_DEBUG_MAKE_SOCKET,                                            \
     "removeSocket, have a request, make a socket")                           \
  XX(HTTP_CLIENT_DOMAIN_NAME, "Unable to determine the domain name")          \
  XX(HTTP_CLIENT_UNESCAPED_PATH,                                              \
    "Request path contains unescaped characters")                             \
  XX(HTTP_CLIENT_UNEXPECTED_PROTOCOL,                                         \
    "Protocol '{0}' not supported. Expected '{1}'")                           \
  XX(HTTP_INVALID_TOKEN, "'{0}' must be a valid HTTP token")                  \
  XX(NET_SOCKET_HANGUP, "socket hang up")                                     \
  XX(HTTP_CLIENT_DEBUG_CLIENT_CREATE, "CLIENT use net.createConnection")      \
  XX(HTTP_CLIENT_DEBUG_SOCKET_CLOSE, "HTTP socket close")                     \
  XX(HTTP_CLIENT_DEBUG_SOCKET_ERROR, "SOCKET ERROR")                          \
  XX(HTTP_CLIENT_DEBUG_SOCKET_ERROR_FREE, "SOCKET ERROR on FREE socket")      \
  XX(HTTP_CLIENT_DEBUG_PARSE_ERROR, "parse error")                            \
  XX(HTTP_CLIENT_DEBUG_SETTING_RESDOMAIN, "setting \"res.domain\"")           \
  XX(HTTP_CLIENT_DEBUG_INCOMING_RESPONSE, "AGENT incoming response!")         \
  XX(HTTP_CLIENT_DEBUG_ISHEAD, "AGENT isHeadResponse")                        \
  XX(HTTP_CLIENT_DEBUG_SOCKET_DESTROYSOON, "AGENT socket.destroySoon()")      \
  XX(HTTP_CLIENT_DEBUG_KEEPALIVE, "AGENT socket keep-alive")                  \
  XX(INVALID_ARG_TYPE, "'{0}' argument must be a {1}")                        \
  XX(REQUIRED_ARG, "'{0}' argument is required")                              \
  XX(HTTP_OUTGOING_SET_AFTER_SEND,                                            \
     "Cannot set headers after they have already been sent")                  \
  XX(HTTP_OUTGOING_REMOVE_AFTER_SEND,                                         \
     "Cannot remove headers after they have already been sent")               \
  XX(HTTP_OUTGOING_RENDER_AFTER_SEND,                                         \
      "Cannot render headers after they have already been sent")              \
  XX(WRITE_AFTER_END, "write after end")                                      \
  XX(FIRST_ARGUMENT_STRING_OR_BUFFER,                                         \
     "The first argument must be a string or Buffer")                         \
  XX(HTTP_OUTGOING_DEBUG_NOT_USE_CHUNKED,                                     \
    "{0} response should not use chunked encoding, closing connection.")      \
  XX(HTTP_OUTGOING_DEBUG_BOTH_REMOVED,                                        \
     "Both Content-Length and Transfer-Encoding are removed")                 \
  XX(HTTP_OUTGOING_DEBUG_IGNORING_WRITE,                                      \
     "This type of response MUST NOT have a body. Ignoring write() calls.")   \
  XX(HTTP_OUTGOING_DEBUG_WRITE_RET, "write ret")                              \
  XX(HTTP_OUTGOING_DEBUG_IGNORING_END_DATA,                                   \
    "This type of response MUST NOT have a body. "                            \
    "Ignoring data passed to end().")                                         \
  XX(HTTP_OUTGOING_MESSAGE_END, "outgoing message end.")                      \
  XX(HTTP_OUTGOING_FLUSH_DEPRECATE,                                           \
    NODE_DEPRECATE_MESSAGE("OutgoingMessage.flush", "flushHeaders"))          \
  XX(HTTP_SERVER_DEBUG_SOCKET_CLOSE, "server socket close")                   \
  XX(HTTP_SERVER_DEBUG_NEW_CONNECTION, "SERVER new http connection")          \
  XX(HTTP_SERVER_DEBUG_SOCKETONDATA, "SERVER socketOnData {0}")               \
  XX(HTTP_SERVER_DEBUG_SOCKETONPARSEREXECUTE,                                 \
     "SERVER socketOnParserExecute {0}")                                      \
  XX(HTTP_SERVER_DEBUG_UPGRADE_OR_CONNECT, "SERVER upgrade or connect")       \
  XX(HTTP_SERVER_DEBUG_HAVE_LISTENER, "SERVER have listener for {0}")         \
  XX(HTTP_SERVER_DEBUG_PAUSE_PARSER, "pause parser")                          \
  XX(LINKLIST_DEPRECATED,                                                     \
    NODE_DEPRECATE_MESSAGE("_linklist module", "a userland alternative"))     \
  XX(STREAM_READABLE_PUSH_AFTER_END, "stream.push() after EOF")               \
  XX(STREAM_READABLE_UNSHIFT_AFTER_END, "stream.unshift() after end event")   \
  XX(STREAM_READABLE_INVALID_CHUNK, "Invalid non-string/buffer chunk")        \
  XX(NOT_IMPLEMENTED, "not implemented")                                      \
  XX(STREAM_READABLE_STREAM_NOT_EMPTY,                                        \
    "'endReadable()' called on non-empty stream")                             \
  XX(STREAM_READABLE_DEBUG_NEED_READABLE, "need readable")                    \
  XX(STREAM_READABLE_DEBUG_LESS_THAN_WATERMARK, "length less than watermark") \
  XX(STREAM_READABLE_DEBUG_READING_OR_ENDED, "reading or ended")              \
  XX(STREAM_READABLE_DEBUG_DO_READ, "do read")                                \
  XX(STREAM_READABLE_DEBUG_EMIT_READABLE, "emit readable")                    \
  XX(STREAM_READABLE_DEBUG_MAYBE_READMORE, "maybeReadMore read 0")            \
  XX(STREAM_READABLE_DEBUG_FALSE_WRITE, "false write response, pause")        \
  XX(STREAM_READABLE_DEBUG_PIPE_RESUME, "pipe resume")                        \
  XX(STREAM_READABLE_DEBUG_READABLE_NEXTTICK, "readable nexttick read 0")     \
  XX(STREAM_READABLE_DEBUG_RESUME_READ, "resume read 0")                      \
  XX(STREAM_READABLE_CALL_PAUSE_FLOWING, "call pause flowing")                \
  XX(STREAM_READABLE_DEBUG_WRAPPED_END, "wrapped end")                        \
  XX(STREAM_READABLE_DEBUG_WRAPPED_DATA, "wrapped data")                      \
  XX(STREAM_READABLE_DEBUG_WRAPPED_READ, "wrapped _read")                     \

#endif  // SRC_NODE_MESSAGES_SRC_H_
