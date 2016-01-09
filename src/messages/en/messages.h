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
  XX(INDEX_OUT_OF_RANGE, "index of out range")                                \
  XX(OUT_OF_BOUNDS_WRITE, "Attempt to write outside bounds")                  \
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
  XX(HTTP_AGENT_DEBUG_SOCKETS, "sockets {0} {1}")                                     \
  XX(HTTP_AGENT_DEBUG_CLIENT_CLOSE, "CLIENT socket onClose")                  \
  XX(HTTP_AGENT_DEBUG_CLIENT_REMOVE, "CLIENT socket onRemove")                \
  XX(HTTP_AGENT_DEBUG_DESTROYED, "removeSocket {0} destroyed {1}")            \
  XX(HTTP_AGENT_DEBUG_MAKE_SOCKET,                                            \
     "removeSocket, have a request, make a socket")                           \
  XX(HTTP_AGENT_DEBUG_ONFREE, "agent.on(free) {0}")                           \
  XX(HTTP_AGENT_DEBUG_CREATE_CONNECTION, "createConnection {0} {1}")          \
  XX(HTTP_CLIENT_DOMAIN_NAME, "Unable to determine the domain name")          \
  XX(HTTP_CLIENT_UNESCAPED_PATH,                                              \
    "Request path contains unescaped characters")                             \
  XX(HTTP_CLIENT_UNEXPECTED_PROTOCOL,                                         \
    "Protocol '{0}' not supported. Expected '{1}'")                           \
  XX(HTTP_INVALID_TOKEN, "'{0}' must be a valid HTTP token")                  \
  XX(NET_SOCKET_HANGUP, "socket hang up")                                     \
  XX(HTTP_CLIENT_DEBUG_CLIENT_CREATE, "CLIENT use net.createConnection {0}")  \
  XX(HTTP_CLIENT_DEBUG_SOCKET_CLOSE, "HTTP socket close")                     \
  XX(HTTP_CLIENT_DEBUG_SOCKET_ERROR, "SOCKET ERROR {0} {1}")                  \
  XX(HTTP_CLIENT_DEBUG_SOCKET_ERROR_FREE,                                     \
    "SOCKET ERROR on FREE socket {0} {1}")                                    \
  XX(HTTP_CLIENT_DEBUG_PARSE_ERROR, "parse error")                            \
  XX(HTTP_CLIENT_DEBUG_SETTING_RESDOMAIN, "setting \"res.domain\"")           \
  XX(HTTP_CLIENT_DEBUG_INCOMING_RESPONSE, "AGENT incoming response!")         \
  XX(HTTP_CLIENT_DEBUG_ISHEAD, "AGENT isHeadResponse {0}")                    \
  XX(HTTP_CLIENT_DEBUG_SOCKET_DESTROYSOON, "AGENT socket.destroySoon()")      \
  XX(HTTP_CLIENT_DEBUG_KEEPALIVE, "AGENT socket keep-alive")                  \
  XX(INVALID_ARG_TYPE, "'{0}' argument must be a(n) {1}")                     \
  XX(INVALID_OPTION_TYPE, "'{0}' option must be a(n) {1}")                    \
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
  XX(HTTP_OUTGOING_DEBUG_WRITE_RET, "write ret = {0}")                        \
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
  XX(HTTP_SERVER_DEBUG_UPGRADE_OR_CONNECT, "SERVER upgrade or connect {0}")   \
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
  XX(STREAM_READABLE_DEBUG_NEED_READABLE, "need readable {0}")                \
  XX(STREAM_READABLE_DEBUG_LESS_THAN_WATERMARK,                               \
     "length less than watermark {0}")                                        \
  XX(STREAM_READABLE_DEBUG_READING_OR_ENDED, "reading or ended {0}")          \
  XX(STREAM_READABLE_DEBUG_DO_READ, "do read")                                \
  XX(STREAM_READABLE_DEBUG_EMIT_READABLE_ARG, "emit readable {0}")            \
  XX(STREAM_READABLE_DEBUG_EMIT_READABLE, "emit readable")                    \
  XX(STREAM_READABLE_DEBUG_MAYBE_READMORE, "maybeReadMore read 0")            \
  XX(STREAM_READABLE_DEBUG_FALSE_WRITE, "false write response, pause {0}")    \
  XX(STREAM_READABLE_DEBUG_PIPE_RESUME, "pipe resume")                        \
  XX(STREAM_READABLE_DEBUG_READABLE_NEXTTICK, "readable nexttick read 0")     \
  XX(STREAM_READABLE_DEBUG_RESUME_READ, "resume read 0")                      \
  XX(STREAM_READABLE_CALL_PAUSE_FLOWING, "call pause flowing = {0}")          \
  XX(STREAM_READABLE_DEBUG_WRAPPED_END, "wrapped end")                        \
  XX(STREAM_READABLE_DEBUG_WRAPPED_DATA, "wrapped data")                      \
  XX(STREAM_READABLE_DEBUG_WRAPPED_READ, "wrapped _read {0}")                 \
  XX(STREAM_READABLE_DEBUG_READ, "read {0}")                                  \
  XX(STREAM_READABLE_DEBUG_PIPE_COUNT, "pipe count={0} opts={1}")             \
  XX(STREAM_READABLE_DEBUG_ONUNPIPE, "onunpipe")                              \
  XX(STREAM_READABLE_DEBUG_ONEND, "onend")                                    \
  XX(STREAM_READABLE_DEBUG_CLEANUP, "cleanup")                                \
  XX(STREAM_READABLE_DEBUG_ONDATA, "ondata")                                  \
  XX(STREAM_READABLE_DEBUG_ONERROR, "onerror")                                \
  XX(STREAM_READABLE_DEBUG_ONFINISH, "onfinish")                              \
  XX(STREAM_READABLE_DEBUG_UNPIPE, "unpipe")                                  \
  XX(STREAM_READABLE_DEBUG_PIPEONDRAIN, "pipeOnDrain {0}")                    \
  XX(STREAM_READABLE_DEBUG_RESUME, "resume")                                  \
  XX(STREAM_READABLE_DEBUG_PAUSE, "pause")                                    \
  XX(STREAM_READABLE_DEBUG_FLOW, "flow")                                      \
  XX(STREAM_READABLE_READEMITREADABLE, "read: emitReadable {0} {1}")          \
  XX(STREAM_TRANSFORM_NO_WRITECB, "no writecb in Transform class")            \
  XX(STREAM_TRANSFORM_DONE_NOT_EMPTY,                                         \
    "Calling transform done when ws.length != 0")                             \
  XX(STREAM_TRANSFORM_DONE_STILL_GOING,                                       \
    "Calling transform done when still transforming")                         \
  XX(STREAM_WRAP_HAS_STRINGDECODER, "Stream has StringDecoder")               \
  XX(STREAM_WRAP_DEBUG_CLOSE, "close")                                        \
  XX(STREAM_WRAP_DEBUG_DATA, "data {0}")                                      \
  XX(STREAM_WRAP_DEBUG_END, "end")                                            \
  XX(STREAM_WRITABLE_CANNOT_PIPE_NOT_READABLE, "Cannot pipe, not readable")   \
  XX(UNKNOWN_ENCODING, "Unknown encoding: {0}")                               \
  XX(STREAM_WRITABLE_WRITABLESTATE_BUFFER_DEPRECATED,                         \
    NODE_DEPRECATE_MESSAGE("_writableState.buffer",                           \
                           "_writableState.getBuffer"))                       \
  XX(NO_OPENSSL, "Node.js was not compiled with OpenSSL crypto support")      \
  XX(TLS_RENEGOTIATION_ATTACK, "TLS session renegotiation attack detected")   \
  XX(TLS_LEGACY_DEBUG_ONFINISH, "{0}.onfinish")                               \
  XX(TLS_LEGACY_DEBUG_ONEND, "cleartext.onend")                               \
  XX(TLS_LEGACY_DEBUG_WRITE, "{0}.write called with {1} bytes")               \
  XX(TLS_LEGACY_DEBUG_WRITE_SUCCEED, "{0}.write succeed with {1} bytes")      \
  XX(TLS_LEGACY_DEBUG_CLEARTEXT_WRITE_QUEUE_FULL,                             \
    "cleartext.write queue is full")                                          \
  XX(TLS_LEGACY_DEBUG_WRITE_QUEUE_BYTES, "{0}.write queued with {1} bytes")   \
  XX(TLS_LEGACY_DEBUG_READ_CALLED, "{0}.read called with {1} bytes")          \
  XX(TLS_LEGACY_DEBUG_READ_SUCCEED, "{0}.read succeed with {1} bytes")        \
  XX(TLS_LEGACY_DEBUG_OUTEND, "{0}.sslOutEnd")                                \
  XX(TLS_LEGACY_DEBUG_END, "{0}.end")                                         \
  XX(TLS_LEGACY_DEBUG_DESTROYSOON, "{0}.destroySoon")                         \
  XX(TLS_LEGACY_DEBUG_DESTROY, "{0}.destroy")                                 \
  XX(TLS_LEGACY_DEBUG_SECUREPAIR_DESTROY, "SecurePair.destroy")               \
  XX(TLS_DEBUG_ONHANDSHAKESTART, "onhandshakestart")                          \
  XX(TLS_DEBUG_ONHANDSHAKEONE, "onhandshakedone")                             \
  XX(TLS_DEBUG_SECURE_ESTABLISHED, "secure established")                      \
  XX(TLS_DEBUG_START, "start")                                                \
  XX(TLS_CALLBACK_CALLED_TWICE, "callback was called more than once")         \
  XX(SOCKET_CLOSED, "Socket is closed")                                       \
  XX(TLS_FAILED_TO_RENEGOTIATE, "Failed to renegotiate")                      \
  XX(TLS_HANDSHAKE_TIMEOUT, "TLS handshake timeout")                          \
  XX(TLS_DH_PARAMETER_SIZE_SMALL,  "DH parameter size {0} is less than {1}")  \
  XX(ASSERT_MISSING_EXCEPTION, "Missing expected exception{0}")               \
  XX(ASSERT_UNWANTED_EXCEPTION, "Got unwanted exception{0}")                  \
  XX(BUFFER_ENCODING_STR,                                                     \
     "If encoding is specified then the first argument must be a string")     \
  XX(BUFFER_INVALID_ARG, "'{0}' must be a number, Buffer, array or string")   \
  XX(BUFFER_INVALID_ARG2, "'{0}' must be a string, number, or Buffer")        \
  XX(BUFFER_INVALID_ARGS, "Arguments must be Buffers")                        \
  XX(BUFFER_TOSTRING_FAILED, "'toString()' failed")                           \
  XX(BUFFER_OUTOFBOUNDS_ARG, "'{0}' argument is out of bounds")               \
  XX(BUFFER_GET_DEPRECATED,                                                   \
     NODE_DEPRECATE_MESSAGE("Buffer.get", "array indices"))                   \
  XX(BUFFER_SET_DEPRECATED,                                                   \
     NODE_DEPRECATE_MESSAGE("Buffer.set", "array indices"))                   \
  XX(BUFFER_WRITE_DEPRECATED,                                                 \
     NODE_DEPRECATE_MESSAGE(                                                  \
       "Buffer.write(string, encoding, offset, length)",                      \
       "write(string[, offset[, length]][, encoding])"))                      \
  XX(CHILD_PROCESS_ARG_OPTION, "Incorrect value of args option")              \
  XX(CHILD_PROCESS_COMMAND_FAILED, "Command failed: {0}\n{1}")                \
  XX(CHILD_PROCESS_MAXBUFFER_EXCEEDED, "{0} maxBuffer exceeded")              \
  XX(CHILD_PROCESS_INVALID_STDIO,                                             \
     "stdio{0} should be Buffer or string not {1}")                           \
  XX(CHILD_PROCESS_DEBUG_SPAWN, "spawn {0} {1}")                              \
  XX(CHILD_PROCESS_DEBUG_SPAWNSYNC, "spawnSync {0} {1}")                      \
  XX(CHILD_PROCESS_CUSTOMFDS_DEPRECATE,                                       \
    NODE_DEPRECATE_MESSAGE("options.customFds", "options.stdio"))             \
  XX(CHILD_PROCESS_SPAWNSYNC, "spawnSync {0}")                                \
  XX(CLUSTER_BAD_SCHEDULING, "Bad cluster.schedulingPolicy: {0}")             \
  XX(CONSOLE_EXPECTS_READABLE, "Console expects a writable stream instance")  \
  XX(CONSOLE_NO_SUCH_LABEL, "No such label: {0}")                             \
  XX(CRYPTO_NO_KEY, "No key provided to sign")                                \
  XX(CRYPTO_BAD_FORMAT, "Bad format: {0}")                                    \
  XX(CRYPTO_CREATECREDENTIALS_DEPRECATED,                                     \
     NODE_DEPRECATE_MESSAGE("crypto.createCredentials",                       \
                            "tls.createSecureContext"))                       \
  XX(CRYPTO_CREDENTIALS_DEPRECATED,                                           \
     NODE_DEPRECATE_MESSAGE("crypto.Credentials", "tls.createSecureContext")) \
  XX(CRYPTO_CANNOT_CHANGE_ENCODING, "Cannot change encoding")                 \
  XX(DGRAM_UNIX_DGRAM_NOT_SUPPORTED,                                          \
    "'unix_dgram' type sockets are no longer supported")                      \
  XX(DGRAM_BAD_SOCKET_TYPE,                                                   \
     "Bad socket type specified. Valid types are: udp4, udp6")                \
  XX(SOCKET_BOUND, "Socket is already bound")                                 \
  XX(DGRAM_SEND_BAD_ARGUMENTS,                                                \
     "send() takes 'offset' and 'length' the second and third arguments")     \
  XX(DGRAM_MUST_SEND_TO_ADDRESS_PORT,                                         \
     "{0} sockets must send to port, address")                                \
  XX(DGRAM_OFFSET_TOO_LOW, "Offset should be >= 0")                           \
  XX(DGRAM_OFFSET_TOO_HIGH, "Offset into buffer is too large")                \
  XX(DGRAM_LENGTH_TOO_LOW, "Length should be >= 0")                           \
  XX(DGRAM_OFFSET_LENGTH_OVER, "Offset + length beyond buffer length")        \
  XX(PORT_OUT_OF_RANGE, "Port should be > 0 and < 65536")                     \
  XX(REQUIRED_ARGUMENT, "{0} argument must be specified")                     \
  XX(NOT_RUNNING, "Not running")                                              \
  XX(DNS_HINTS_VALUE, "'{0}' must be a valid flag")                           \
  XX(DNS_FAMILY_VALUE, "'{0}' must be 4 or 6")                                \
  XX(DNS_INVALID_ARGUMENTS, "Invalid arguments")                              \
  XX(DNS_VALID_ID, "'{0}' argument must be a valid IP address'")              \
  XX(DNS_UNKNOWN_TYPE, "Unknown type '{0}'")                                  \
  XX(DNS_MALFORMED_IP, "IP address is not properly formatted: {0}")           \
  XX(DNS_FAILED_TO_SET_SERVERS, "c-ares failed to set servers: {0} [{1}]")    \
  XX(ARGUMENT_POSITIVE_NUMBER, "'{0}' must be a positive number")             \
  XX(UNCAUGHT_UNSPECIFIED_ERROR, "Uncaught, unspecified 'error' event")       \
  XX(UNCAUGHT_UNSPECIFIED_ERROR2, "Uncaught, unspecified 'error' event ({0})")\
  XX(EVENT_MEMORY_LEAK,                                                       \
    "warning: possible EventEmitter memory "                                  \
    "leak detected. {0} {1} listeners added."                                 \
    "Use emitter.setMaxListeners() to increase limit.")                       \
  XX(ARGUMENT_STRING_OR_OBJECT, "'{0}' argument must be a string or object")  \
  XX(PATH_NULL_BYTES, "'{0}' must be a string without null bytes")            \
  XX(FS_SIZE_TOO_LARGE,                                                       \
     "File size is greater than possible Buffer: 0x{0} bytes")                \
  XX(FS_UNKNOWN_FLAG, "Unknown file open flag: {0}")                          \
  XX(FS_MALFORMED_TIME, "Cannot parse time: {0}")                             \
  XX(FS_OPTION_LESS_THAN, "'{0}' option must be <= '{1}' option")             \
  XX(FS_OPTION_MORE_THAN_0, "'{0}' must be >= zero")                          \
  XX(FS_INVALID_DATA, "Invalid data")                                         \
  XX(BAD_ARGUMENTS, "Bad arguments")                                          \
  XX(HTTP_CLIENT_DEPRECRATED, "http.Client is deprecated.")                   \
  XX(HTTP_CREATECLIENT_DEPRECATED,                                            \
    NODE_DEPRECATE_MESSAGE("http.createClient", "http.request"))              \
  XX(HTTPS_DEBUG_CREATECONNECTION, "createConnection {0}")                    \
  XX(HTTPS_REUSE_SESSION, "reuse session for {0}")                            \
  XX(HTTPS_EVICTING, "evicting {0}")                                          \
  XX(MODULE_NOT_FOUND, "Cannot find module '{0}'")                            \
  XX(MODULE_DEBUG_RELATIVE_REQUESTED,                                         \
    "RELATIVE: requested: {0} set ID to: {1} from {2}")                       \
  XX(MODULE_DEBUG_LOAD_REQUEST,                                               \
    "Module._load REQUEST {0} parent: {1}")                                   \
  XX(MODULE_DEBUG_LOAD_NATIVE, "load native module {0}")                      \
  XX(MODULE_DEBUG_LOOKING_FOR, "looking for {0} in {1}")                      \
  XX(MODULE_DEBUG_LOAD, "load {0} for module {1}")                            \
  XX(MODULE_ASSERT_MISSING_PATH, "missing path")                              \
  XX(MODULE_ASSERT_PATH_STRING, "path must be a string")                      \
  XX(NET_UNSUPPORTED_FD, "Unsupported fd type: {0}")                          \
  XX(NET_PEER_ENDED, "This socket has been ended by the other party")         \
  XX(NET_INVALID_ADDRESS_TYPE, "Invalid addressType: {0}")                    \
  XX(ARGUMENT_STRING_OR_NUMBER, "'{0}' argument must be a string or number")  \
  XX(NET_INVALID_LISTENER_ARG, "Invalid listen argument: {0}")                \
  XX(NET_DEBUG_ONCONNECTION, "onconnection")                                  \
  XX(NET_DEBUG_SERVER_EMITCLOSEIFDRAINED, "SERVER _emitCloseIfDrained")       \
  XX(NET_DEBUG_SERVER_HANDLE, "SERVER handle? {0}   connections? {1}")        \
  XX(NET_DEBUG_SERVER_EMITCLOSE, "SERVER: emit close")                        \
  XX(NET_DEBUG_CREATECONNECTION, "createConnection {0}")                      \
  XX(NET_DEBUG_NOTYETCONNECTED, "osF: not yet connected")                     \
  XX(NET_DEBUG_ONSOCKETFINISH, "onSocketFinish")                              \
  XX(NET_DEBUG_OSFENDED, "oSF: ended, destroy {0}")                           \
  XX(NET_DEBUG_OFSNOTENDED, "oSF: not ended, call shutdown()")                \
  XX(NET_DEBUG_AFTERSHUTDOWN, "afterShutdown destroyed={0} {1}")              \
  XX(NET_DEBUG_READABLESTATEENDED, "readableState ended, destroying")         \
  XX(NET_DEBUG_ONSOCKET_END, "onSocketEnd {0}")                               \
  XX(NET_DEBUG_SOCKETLISTEN, "socket.listen")                                 \
  XX(NET_DEBUG_ONTIMEOUT, "_onTimeout")                                       \
  XX(NET_DEBUG_READ, "_read")                                                 \
  XX(NET_DEBUG_READWAITFORCON, "_read wait for connection")                   \
  XX(NET_DEBUG_READSTART, "Socket._read readStart")                           \
  XX(NET_DEBUG_DESTROY, "destroy")                                            \
  XX(NET_DEBUG_ALREADYDESTROYED, "already destroyed, fire error callbacks")   \
  XX(NET_DEBUG_CLOSE, "close")                                                \
  XX(NET_DEBUG_CLOSEHANDLE, "close handle")                                   \
  XX(NET_DEBUG_EMITCLOSE, "emit close")                                       \
  XX(NET_DEBUG_HASSERVER, "has server")                                       \
  XX(NET_DEBUG_DESTROY_ERR, "destroy {0}")                                    \
  XX(NET_DEBUG_ONREAD, "onread")                                              \
  XX(NET_DEBUG_GOTDATA, "got data")                                           \
  XX(NET_DEBUG_READSTOP, "readStop")                                          \
  XX(NET_DEBUG_NODATA_KEEPREADING, "not any data, keep waiting")              \
  XX(NET_DEBUG_EOF, "EOF")                                                    \
  XX(NET_DEBUG_AFTERWRITE, "afterWrite {0}")                                  \
  XX(NET_DEBUG_AFTERWRITE_DESTROYED, "afterWrite destroyed")                  \
  XX(NET_DEBUG_WRITEFAILURE, "write failure {0}")                             \
  XX(NET_DEBUG_AFTERWRITECB, "afterWrite call cb")                            \
  XX(NET_DEBUG_BINDING, "binding to localAddress: {0} and localPort: {1}")    \
  XX(NET_DEBUG_PIPE, "pipe {0} {1}")                                          \
  XX(NET_DEBUG_CONNECT_FINDHOST, "connect: find host {0}")                    \
  XX(NET_DEBUG_CONNECT_DNSOPTS, "connect: dns options {0}")                   \
  XX(NET_DEBUG_AFTERCONNECT, "afterConnect")                                  \
  XX(NET_DEBUG_INVALIDFD, "listen invalid fd={0}:{1}")                        \
  XX(NET_DEBUG_BINDTO, "bind to {0}")                                         \
  XX(NET_DEBUG_LISTEN2, "listen2 {0} {1} {2} {3} {4}")                        \
  XX(NET_DEBUG_LISTEN2_HAVE_HANDLE, "_listen2: have a handle already")        \
  XX(NET_DEBUG_LISTEN2_CREATE_HANDLE, "_listen2: create a handle")            \
  XX(OS_GETNETWORKINTERFACES_DEPRECATED,                                      \
     NODE_DEPRECATE_MESSAGE("os.getNetworkInterfaces",                        \
                            "os.networkInterfaces"))                            \
  XX(PUNYCODE_OVERFLOW, "Overflow: input needs wider integers to process")    \
  XX(PUNYCODE_NOTBASIC, "Illegal input >= 0x80 (not a basic code point)")     \
  XX(PUNYCODE_INVALIDINPUT, "Invalid input")                                  \
  XX(READLINE_CANNOT_SET_COL,                                                 \
     "Cannot set cursor row without also setting the column")                 \
  XX(READLINE_CODEPOINT_DEPRECATED,                                           \
     NODE_DEPRECATE_MESSAGE("readline.codePointAt",                           \
                            "String.prototype.codePointAt"))                  \
  XX(READLINE_GETSTRINGWIDTH_DEPRECATED,                                      \
     "getStringWidth is deprecated and will be removed.")                     \
  XX(READLINE_ISFULLWIDTHCODEPOINT_DEPRECATED,                                \
     "isFullWidthCodePoint is deprecated and will be removed.")               \
  XX(READLINE_STRIPVTCONTROLCHARACTERS_DEPRECATED,                            \
     "stripVTControlCharacters is deprecated and will be removed.")           \
  XX(REPL_OPTIONS_OR_PROMPT_REQUIRED,                                         \
     "An options object, or a prompt string are required")                    \
  XX(REPL_DEBUG_PARSE_ERROR, "parse error {0} {1}")                           \
  XX(REPL_DEBUG_NOT_RECOVERABLE, "not recoverable, send to domain")           \
  XX(REPL_DEBUG_DOMAIN_ERROR, "domain error")                                 \
  XX(REPL_DEBUG_LINE, "line {0}")                                             \
  XX(REPL_DEBUG_EVAL, "eval {0}")                                             \
  XX(REPL_DEBUG_FINISH, "finish {0} {1}")                                     \
  XX(REPL_NPM_OUTSIDE,                                                        \
    "npm should be run outside of the node repl, in your normal shell.\n"     \
    "(Press Control-D to exit.)")                                             \
  XX(REPL_BLOCK_SCOPED,                                                       \
    "Block-scoped declarations (let, const, function, class) "                \
    "not yet supported outside strict mode")                                  \
  XX(REPL_EXIT, "(To exit, press ^C again or type .exit)")                    \
  XX(REPL_INVALID_KEYWORD, "Invalid REPL keyword")                            \
  XX(REPL_COMMAND_BREAK, "Sometimes you get stuck, this gets you out")        \
  XX(REPL_COMMAND_CLEAR_GLOBAL, "Alias for .break")                           \
  XX(REPL_COMMAND_CLEAR, "Break, and also clear the local context")           \
  XX(REPL_COMMAND_CLEAR_MSG, "Clearing context...")                           \
  XX(REPL_COMMAND_EXIT, "Exit the repl")                                      \
  XX(REPL_COMMAND_HELP, "Show repl options")                                  \
  XX(REPL_COMMAND_SAVE,                                                       \
    "Save all evaluated commands in this REPL session to a file")             \
  XX(REPL_COMMAND_SESSION_SAVED, "Session saved to:{0}")                      \
  XX(REPL_COMMAND_SAVE_FAILED, "Failed to save:{0}")                          \
  XX(REPL_COMMAND_LOAD, "Load JS from a file into the REPL session")          \
  XX(REPL_COMMAND_LOAD_FAILED_INVALID,                                        \
     "Failed to load:{0} is not a valid file")                                \
  XX(REPL_COMMAND_LOAD_FAILED, "Failed to load:{0}")                          \
  XX(SYS_DEPRECATED, NODE_DEPRECATE_MESSAGE("sys", "util"))                   \
  XX(TIMERS_NON_NEGATIVE_FINITE,                                              \
     "'{0}' argument must be a non-negative finite number")                   \
  XX(TIMERS_DEBUG_CALLBACK, "timeout callback {0}")                           \
  XX(TIMERS_DEBUG_NOW, "now: {0}")                                            \
  XX(TIMERS_DEBUG_LIST_WAIT, "{0} list wait because diff is {1}")             \
  XX(TIMERS_DEBUG_LIST_EMPTY, "{0} list empty")                               \
  XX(TIMERS_DEBUG_REUSE_HIT, "reuse hit")                                     \
  XX(TIMERS_DEBUG_UNENROLL, "unenroll: list empty")                           \
  XX(TIMERS_DEBUG_UNREFTIMER_TIMEOUT, "unreftimer firing timeout")            \
  XX(TIMERS_DEBUG_UNREFTIMER_FIRED, "unrefTimer fired")                       \
  XX(TIMERS_DEBUG_UNREFTIMER_RESCHED, "unrefTimer rescheduled")               \
  XX(TIMERS_DEBUG_UNREFLIST_EMPTY, "unrefList is empty")                      \
  XX(TIMERS_DEBUG_UNREFLIST_INIT, "unrefList initialized")                    \
  XX(TIMERS_DEBUG_UNREFTIMER_INIT, "unrefTimer initialized")                  \
  XX(TIMERS_DEBUG_UNREFTIMER_SCHED, "unrefTimer scheduled")                   \
  XX(TIMERS_DEBUG_UNREFLIST_APPEND, "unrefList append to end")                \
  XX(TLS_HOSTNAME_NO_MATCH,                                                   \
     "Hostname/IP doesn't match certificate's altnames: '{0}'")               \
  XX(TLS_UNKNOWN_REASON, "Unknown reason")                                    \
  XX(TLS_HOST_NOT_IN_CERT_LIST, "IP: {0} is not in the cert's list: {1}")     \
  XX(TLS_HOST_NOT_IN_CERT_ALTNAMES,                                           \
     "Host: {0} is not in the cert's altnames: {1}")                          \
  XX(TLS_HOST_NOT_IN_CERT_CN, "Host: {0} is not cert's CN: {1}")              \
  XX(TLS_CERT_IS_EMPTY, "Cert is empty")                                      \

#endif  // SRC_NODE_MESSAGES_SRC_H_
