#Node.js notable changes between LTS versions 10 and 12

Below listed is the complete set of notable changes
(major features and breaking changes) between version 10 and 12.
This is inclusive of V10.1.0 and V12.0.0

console:
  make console.table() use colored inspect
  (TSUYUSATO Kitsune) #20510
  The console.timeLog() method has been implemented. #21312
  console.countReset() will emit a warning if the timer being
  reset does not exist. #21649
  console.time() will no longer reset a timer if it
  already exists #20442
  Added inspectOptions option (Ruben Bridgewater) #24978

fs:
  move fs/promises to fs.promises (cjihrig) #20504
  fix reads with pos > 4GB (Mathias Buus) #21003
  BigInt support has been added to fs.stat and fs.watchFile. #20220
  APIs that take mode as arguments no longer throw on values
  larger than 0o777. #20636 #20975 (Fixes: #20498)
  Fix crashes in closed event watchers. #20985 (Fixes: #20297)
  fs.lchown has been undeprecated now that libuv supports it. #21498
  The methods fs.read, fs.readSync, fs.write, fs.writeSync,
  fs.writeFile and fs.writeFileSync now all accept TypedArray
  and DataView objects. #22150
  A new boolean option, withFileTypes, can be passed to fs.readdir
  and fs.readdirSync. If set to true, the methods return an array of
  directory entries. These are objects that can be used to determine
  the type of each entry and filter them based on that without calling
  fs.stat. #22020
  Fixed fsPromises.readdir withFileTypes. #22832
  Added a recursive option to fs.mkdir and fs.mkdirSync. If this
  option is set to true, non-existing parent folders will be
  automatically created. #21875
  remove experimental warning for fs.promises (Anna Henningsen) #26581
  The fs.read() method now requires a callback. #22146
  The previously deprecated fs.SyncWriteStream utility has been
  removed.#20735
  Use internalBinding('fs') internally instead of
  process.binding('fs') (Masashi Hirano) #22478
  remove experimental warning for fs.promises (Anna Henningsen) #26581

http:
  added aborted property to request (Robert Nagy) #20094
  Handling of close and aborted events has been made more consistent.
  (Robert Nagy) #20075, #20611
  Added support for passing both timeout and agent options to
  http.request. #21204
  http.get() and http.request() (and https variants) can now
  accept three arguments to allow for a URL and an options
  object (Sam Ruby) #21616
  Headers received by HTTP servers must not exceed 8192 bytes in
  total to prevent possible Denial of Service attacks. Reported
  by Trevor Norris. (CVE-2018-12121 / Matteo Collina)
  A timeout of 40 seconds now applies to servers receiving HTTP
  headers. This value can be adjusted with server.headersTimeout.
  Where headers are not completely received within this period,
  the socket is destroyed on the next received chunk. In conjunction
  with server.setTimeout(), this aids in protecting against
  excessive resource retention and possible Denial of Service.
  Reported by Jan Maybach (liebdich.com).
  (CVE-2018-12122 / Matteo Collina)
  add maxHeaderSize property (cjihrig) #24860
  Further prevention of "Slowloris" attacks on HTTP and HTTPS
  connections by consistently applying the receive timeout
  set by server.headersTimeout to connections in keep-alive mode.
  Reported by Marco Pracucci (Voxnest).
  (CVE-2019-5737 / Matteo Collina)
  fix error check in Execute() (Brian White) #25863
  makes response.writeHead return the response
  (Mark S. Everitt) #25974
  The http, https, and tls modules now use the WHATWG URL parser
  by default. #20270
  Headers received by HTTP servers must not exceed 8192 bytes in
  total to prevent possible Denial of Service attacks. Reported by
  Trevor Norris. (CVE-2018-12121 / Matteo Collina)
  A timeout of 40 seconds now applies to servers receiving HTTP
  headers. This value can be adjusted with server.headersTimeout.
  Where headers are not completely received within this period,
  the socket is destroyed on the next received chunk.
  In conjunction with server.setTimeout(), this aids in protecting
  against excessive resource retention and possible Denial of Service.
  Reported by Jan Maybach (liebdich.com).
  (CVE-2018-12122 / Matteo Collina)
  Chosing between the http parser is now possible per runtime flag.
  https://github.com/nodejs/node/pull/24739
  add maxHeaderSize property (cjihrig) #24860
  response.writeHead now returns the response object. #25974
  Support overriding http\s.globalAgent (Roy Sommer) #25170

n-api:
  initialize a module via a special symbol (Gabriel Schulhof) #20161
  Prevent use-after-free in napi_delete_async_work
  Add API for asynchronous functions. #17887
  mark thread-safe function as stable (Gabriel Schulhof) #25556
  make func argument of napi_create_threadsafe_function optional
  (legendecas) #27791
  mark version 5 N-APIs as stable (Gabriel Schulhof) #29401
  implement date object (Jarrod Connolly) #25917
  The napi_threadsafe_function feature is now stable. #25556
  Implement date object (Jarrod Connolly) #25917
  Added experimental support for functions dealing with
  bigint numbers. #21226

src:
  add public API to expose the main V8 Platform
  (Allen Yonghuang Wang) #20447
  Deprecated option variables in public C++ API. #22515
  Refactored options parsing. #22392
  add .code and SSL specific error properties (Sam Roberts) #25093

addons:
  Fixed a memory leak for users of AsyncResource and N-API.
  (Michael Dawson) #20668

assert:
  The error parameter of assert.throws() can be an object
  containing regular expressions now. (Ruben Bridgewater) #20485
  The diff output is now a tiny bit improved by sorting
  object properties when inspecting the values that are
  compared with each other. #22788

crypto:
  The authTagLength option has been made more flexible.
  (Tobias Nieben) #20235, #20039
  Support for crypto.scrypt() has been added. #20816
  Added support for PEM-level encryption. #23151
  Added an API asymmetric key pair generation. The new methods
  crypto.generateKeyPair and crypto.generateKeyPairSync can be
  used to generate public and private key pairs. The API supports
  RSA, DSA and EC and a variety of key encodings
  (both PEM and DER). #22660
  add support for chacha20-poly1305 for AEAD (chux0519) #24081
  increase maxmem range from 32 to 53 bits (Tobias Nieben) #28799
  always accept certificates as public keys (Tobias Nieben) #24234
  add key object API (Tobias Nieben) #24234
  update root certificates (Sam Roberts) #25113
  Always accept private keys as public keys (Tobias Nieben) #25217
  Allow deriving public from private keys (Tobias Nieben) #26278.

esm:
  Builtin modules (e.g. fs) now provide named exports in
  ES6 modules. (Gus Caplan) #20403

module:
  add --preserve-symlinks-main (David Goldstein) #19911
  Added module.createRequireFromPath(filename). This new method
  can be used to create a custom require function that will
  resolve modules relative to the filename path. #19360

timers:
  timeout.refresh() has been added to the public API.
  (Jeremiah Senkpiel) #20298
  Interval timers will be rescheduled even if previous interval
  threw an error. #20002
  nextTick queue will be run after each immediate and timer. #22842
  Fixed an issue that could cause timers to enter an
  infinite loop. #23870
  Fixed an issue that could cause setTimeout to stop working as
  expected. https://github.com/nodejs/node/pull/24322

Embedder support:
  Functions for creating V8 Isolate and Context objects with
  Node.js-specific behaviour have been added to the API.
  (Allen Yonghuang Wang) #20639
  Node.js Environments clean up resources before exiting now.
  (Anna Henningsen) #19377
  Support for multi-threaded embedding has been improved.
  (Anna Henningsen) #20542, #20539, #20541

deps:
  Upgrade npm to 6.1.0 (Rebecca Turner) #20190
  Update V8 to 6.7.288.43 (Michaël Zasso) #19989
  Upgrade to libuv 1.22.0. #21731
  Upgrade to ICU 62.1 (Unicode 11, CLDR 33.1). #21728
  Upgrade npm to 6.2.0. #21592
    npm has moved. This release updates various URLs
    to point to the right places for bugs, support, and PRs.
    Fix the regular expression matching in xcode_emulation
    in node-gyp to also handle version numbers with multiple-digit
    major versions which would otherwise break under use of XCode 10.
    The npm tree has been significantly flattened. Tarball size
    for the npm package has gone from 8MB to 4.8MB.
    Changelogs: 6.2.0-next.0, 6.2.0-next.1, 6.2.0.
  Upgrade to OpenSSL 1.1.0i, fixing:
    Client DoS due to large DH parameter (CVE-2018-0732)
    ECDSA key extraction via local side-channel (CVE not assigned)
  Upgrade V8 from 6.7 to 6.8 (Michaël Zasso) #21079
    Memory reduction and performance improvements, details
    at: https://v8project.blogspot.com/2018/06/v8-release-68.html
  The bundled npm was upgraded to version 6.4.1. #22591
    Changelogs: 6.3.0-next.0 6.3.0 6.4.0 6.4.1
  Upgrade to OpenSSL 1.1.0j, fixing CVE-2018-0734 and CVE-2019-0735
  Upgrade to c-ares v1.15.0 (Ben Noordhuis) #23854
  Update ICU to 64.2 (Ujjwal Sharma) #27361
  Upgrade npm to 6.9.0 (Kat Marchán) #26244
  Upgrade openssl sources to 1.1.1b (Sam Roberts) #26327
  Upgrade to libuv 1.28.0 (cjihrig) #27241
  Upgrade openssl sources to 1.1.1c (Sam Roberts) #28212
  Update npm to 6.11.3 (claudiahdz) #29430
  Upgrade openssl sources to 1.1.1d (Sam Roberts) #29921
  Update npm to 6.13.4 #30904
  V8 has been updated to 7.0. #22754
  Updated ICU to 63.1. #23715
  A new experimental HTTP parser (llhttp) is now supported.
  https://github.com/nodejs/node/pull/24059
  Upgrade to OpenSSL 1.1.0j, fixing CVE-2018-0734 and CVE-2019-0735
  Upgrade to libuv 1.24.1 (cjihrig) #25078
  Upgrade npm to 6.5.0 (Audrey Eschright) #24734
  Upgrade npm to v6.5.0 (Jordan Harband) #25234
  OpenSSL has been updated to 1.1.1a, which is API/ABI compatible
  with the previous OpenSSL 1.1.0j. Note that while OpenSSL 1.1.1a
  supports TLS1.3, Node.js still does not. #25381
  Updated libuv to 1.26.0. #26037
  Updated npm to 6.7.0. #25804
  Upgrade openssl to 1.1.1b (Sam Roberts) #26327
  Update nghttp2 to 1.37.0 (gengjiawen) #26990
  add s390 asm rules for OpenSSL-1.1.1 (Shigeki Ohtsu) #19794

net:
  New option to allow IPC servers to be readable and writable
  by all users (Bartosz Sosnowski) #19472

stream:
  fix removeAllListeners() for Stream.Readable to work as expected
  when no arguments are passed (Kael Zhang) #20924
  ensure Stream.pipeline re-throws errors without callback
  (Blaine Bublitz) #20437
  fix end-of-stream for HTTP/2 (Anna Henningsen) #24926
  do not unconditionally call _read() on resume() (Anna Henningsen) #26965
  implement Readable.from async iterator utility (Guy Bedford) #27660
  make Symbol.asyncIterator support stable (Matteo Collina) #26989

http2:
  (CVE-2018-7161): Fixes Denial of Service vulnerability by updating
  the http2 implementation to not crash under certain circumstances
  during cleanup
  (CVE-2018-1000168): Fixes Denial of Service vulnerability by
  upgrading nghttp2 to 1.32.0
  The http2 module is no longer experimental. #22466
  Added http2stream.endAfterHeaders property. #22843
  Added a 'ping' event to Http2Session that is emitted whenever
  a non-ack PING is received. #23009
  Added support for the ORIGIN frame. #22956
  Updated nghttp2 to 1.34.0. This adds RFC 8441 extended connect
  protocol support to allow use of WebSockets over HTTP/2. #23284
  makes response.writeHead return the response (Mark S. Everitt) #25974
  response.writeHead now returns the response object. #25974

tls:
  Fixes Denial of Service vulnerability by updating the TLS
  implementation to not crash upon receiving
  throw if protocol too long (Andre Jodat-Danbrani) #23606
  Added min/max protocol version options.
  https://github.com/nodejs/node/pull/24405
  The X.509 public key info now includes the RSA bit size and
  the elliptic curve. https://github.com/nodejs/node/pull/24358
  tls.connect() takes a timeout option analogous to
  the net.connect() one. nodejs/node#25517
  support "BEGIN TRUSTED CERTIFICATE" for ca: (Sam Roberts) #24733
  Introduced client "session" event. #25831
  add --tls-min-v1.2 CLI switch (Sam Roberts) #26951
  supported shared openssl 1.1.0 (Sam Roberts) #26951
  revert default max toTLSv1.2 (Sam Roberts) #26951
  revert change to invalid protocol error type (Sam Roberts) #26951
  support TLSv1.3 (Sam Roberts) #26209
  add code for ERR_TLS_INVALID_PROTOCOL_METHOD (Sam Roberts) #24729

Worker Threads:
  Support for multi-threading has been added behind the
  --experimental-worker flag in the worker_threads module.
  This feature is experimental and may receive breaking changes
  at any time. #20876

dns:
  An experimental promisified version of the dns module is now
  available. Give it a try with require('dns').promises. #21264
  remove dns.promises experimental warning (cjihrig) #26592
  make dns.promises enumerable (cjihrig) #26592
  remove dns.promises experimental warning (cjihrig) #26592

lib:
  Atomics.wake is being renamed to Atomics.notify in the ECMAScript
  specification (reference). Since Node.js now has experimental
  support for worker threads, we are being proactive and added
  a notify alias, while emitting a warning if wake is used.
  #21413 #21518

util:
  util.inspect is now able to return a result instead of
  throwing when the maximum call stack size is exceeded during
  inspection. #20725
  Added util.types.isBoxedPrimitive(value). #22620
  Added the sorted option to util.inspect(). If set to true, all
  properties of an object and Set and Map entries will be sorted
  in the returned string. If set to a function, it is used as
  a compare function. #22788
  The util.instpect.custom symbol is now defined in the global
  symbol registry as Symbol.for('nodejs.util.inspect.custom'). #20857
  Added support for BigInt numbers in util.format(). #22097
  The WHATWG TextEncoder and TextDecoder are now globals. #22281
  util.inspect() output size is limited to 128 MB by default. #22756
  A runtime warning will be emitted when NODE_DEBUG is set for
  either http or http2. #21914
  add inspection getter option (Ruben Bridgewater) #24852
  Inspect ArrayBuffers contents closely (Ruben Bridgewater) #25006
  Add compact depth mode for util.inspect() (Ruben Bridgewater)
  #26269

vm:
  Add script.createCachedData(). This API replaces the
  produceCachedData
  option of the Script constructor that is now deprecated. #20300
  Added vm.compileFunction, a method to create new JavaScript
  functions
  from a source body, with options similar to those of
  the other vm methods. #21571

worker:
  Support for relative paths has been added to the Worker constructor.
  Paths are interpreted relative to the current working
  directory. #21407
  Debugging support for Workers using the DevTools protocol
  has been implemented. #21364
  The public inspector module is now enabled in Workers. #22769
  fix nullptr deref after MessagePort deser failure
  (Anna Henningsen) #25076
  Expose workers by default and remove --experimental-worker flag
  (Anna Henningsen) #25361
  process.umask() is available as a read-only function inside Worker
  threads now nodejs/node#25526
  An execArgv option that supports a subset of Node.js command line
  options is supported now. nodejs/node#25467
  Improve integration with native addons (Anna Henningsen) #26175
  MessagePort.prototype.onmessage takes arguments closer to the Web
  specification now (Anna Henningsen) #26082
  Added worker.moveMessagePortToContext. This enables using
  MessagePorts in different vm.Contexts, aiding with the isolation
  that the vm module seeks to provide (Anna Henningsen) #26497.
  use copy of process.env (Anna Henningsen) #26544

inspector:
  Expose the original console API in
  require('inspector').console. #21659

process:
  The process.hrtime.bigint() method has been implemented. #21256
  Added the --title command line argument to set the process title
  on startup. #21477
  Added process.allowedNodeEnvironmentFlags. This object can be used
 to programmatically validate and list flags that are allowed in the
  NODE_OPTIONS environment variable. #19335
  Added a 'multipleResolves' process event that is emitted whenever
  a Promise is attempted to be resolved multiple times, e.g. if the
  resolve and reject functions are both called in a Promise executor.
  #22218
  add --unhandled-rejections flag (Ruben Bridgewater) #26599
  Exposed process.features.inspector. #25819
  Make process[Symbol.toStringTag] writable again
  (Ruben Bridgewater) #26488

trace_events:
  Added process_name metadata. #21477

buffer:
  Fix out-of-bounds (OOB) write in Buffer.write() for UCS-2
  encoding (CVE-2018-12115)
  Fix unintentional exposure of uninitialized memory in
  Buffer.alloc() (CVE-2018-7166)

child_process:
  TypedArray and DataView values are now accepted as input by
  execFileSync and spawnSync. #22409
  The default value of the windowsHide option has been changed
  to true. #21316
  When the maxBuffer option is passed, stdout and stderr
  will be truncated rather than unavailable in case of an error.
  nodejs/node#24951
  doc deprecate ChildProcess._channel (cjihrig) #26982

coverage:
  Native V8 code coverage information can now be output to disk
  by setting the environment variable NODE_V8_COVERAGE to
  a directory. #22527

os:
  Added two new methods: os.getPriority and os.setPriority,
  allowing to manipulate the scheduling priority of processes. #22407

cli:
  The options parser now normalizes _ to - in all multi-word
  command-line flags, e.g. --no_warnings has the same effect
  as --no-warnings. #23020
  Added bash completion for the node binary. To generate a bash
  completion script, run node --completion-bash. The output can be
  saved to a file which can be sourced to enable completion. #20713
  add --max-http-header-size flag (cjihrig) #24811

url:
  Added url.fileURLToPath(url) and url.pathToFileURL(path). These
  methods can be used to correctly convert between file: URLs and
  absolute paths. #22506
  Fix a bug that would allow a hostname being spoofed when parsing
  URLs with url.parse() with the 'javascript:' protocol. Reported
  by Martin Bajanik (Kentico). (CVE-2018-12123 / Matteo Collina)
  pathToFileURL() now supports LF, CR and TAB.
  https://github.com/nodejs/node/pull/23720

V8 API:
  A number of V8 C++ APIs have been marked as deprecated
  since they have been removed in the upstream repository.
  Replacement APIs are added where necessary. #23159

windows:
  The Windows msi installer now provides an option to automatically
  install the tools required to build native modules. #22645
  Revert changes to installer causing issues on Windows systems.
  A crashing process will now show the names of stack frames if
  the node.pdb file is available. (Refael Ackermann) #23822
  Continued effort to improve the installer's new stage that
  installs native build tools.
  https://github.com/nodejs/node/pull/23987,
  https://github.com/nodejs/node/pull/24348
  child_process:
    On Windows the windowsHide option default was restored
    to false. This means detached child processes and GUI apps
    will once again start in a new window.
    https://github.com/nodejs/node/pull/24034
  Tools are not installed using Boxstarter anymore.
  https://github.com/nodejs/node/pull/24677
  The install-tools scripts or now included in the dist.
  https://github.com/nodejs/node/pull/24233

events:
  add once method to use promises with EventEmitter
  (Matteo Collina) #26078
  For unhandled error events with an argument that is not
  an Error object, the resulting exeption will have more
  information about the argument. nodejs/node#25621
  Added a once function to use EventEmitter with promises
  (Matteo Collina) #26078.

repl:
  support top-level for-await-of (Shelley Vohr) #23841
  Top-level for-await-of is now supported in the REPL. #23841
  The multiline history feature is removed.
  https://github.com/nodejs/node/pull/24804
  Added repl.setupHistory for programmatic repl. #25895
  Add util.inspect.replDefaults to customize the writer
  (Ruben Bridgewater) #26375

zlib:
  add brotli support (Anna Henningsen) #24938

build:
  FreeBSD 10 is no longer supported. #22617
  Enable v8's siphash for hash seed creation (Rod Vagg) #26367

readline:
  The readline module now supports async iterators.
  https://github.com/nodejs/node/pull/23916

console, util:
  console functions now handle symbols as defined in the spec.
  https://github.com/nodejs/node/pull/23708
  The inspection depth default is now back at 2.
  https://github.com/nodejs/node/pull/24326

dgram, net:
  Added ipv6Only option for net and dgram.
  https://github.com/nodejs/node/pull/23798

tty:
  Added a hasColors method to WriteStream (Ruben Bridgewater) #26247.
  Added NO_COLOR and FORCE_COLOR support (Ruben Bridgewater) #26485.

policy:
  Experimental support for module integrity checks through
  a manifest file is implemented now. nodejs/node#23834

report:
  An experimental diagnostic API for capturing process state is
  available as process.report and through command line flags.
  nodejs/node#22712
  Rename triggerReport() to writeReport() (Colin Ihrig) #26527

perf_hooks:
  Implemented a histogram based API. #25378

V8:
  Added v8.getHeapSnapshot and v8.writeHeapSnapshot to generate
  snapshots in the format used by tools such as Chrome DevTools
  (James M Snell) #26501.

C++ API:
  AddPromiseHook is now deprecated. This API was added to fill
  an use case that is served by async_hooks, since that has Promise
  support (Anna Henningsen) #26529.
  Added a Stop API to shut down Node.js while it is running
  (Gireesh Punathil) #21283.

General:
  Use of process.binding() has been deprecated. Userland code using
  process.binding() should re-evaluate that use and begin migrating.
  If there are no supported API alternatives, please open an issue
  in the Node.js GitHub repository so that a suitable alternative
  may be discussed.
  An experimental implementation of queueMicrotask()
  has been added. #22951

Internal:
  Windows performance-counter support has been removed. #22485
  The --expose-http2 command-line option has been removed. #20887

bootstrap:
  Add experimental --frozen-intrinsics flag (Guy Bedford) #25685

Fixes memory exhaustion DoS (CVE-2018-7164):
  Fixes a bug introduced in 9.7.0 that increases the memory
  consumed when reading from the network into JavaScript using
  the net.Socket object directly as a stream.

Vulnerabilities fixed:
  CVE-2019-9511 "Data Dribble": The attacker requests a large
  amount of data from a specified resource over multiple
  streams. They manipulate window size and stream priority
  to force the server to queue the data in 1-byte chunks.
  Depending on how efficiently this data is queued, this can
  consume excess CPU, memory, or both, potentially leading
  to a denial of service.
  CVE-2019-9512 "Ping Flood": The attacker sends continual
  pings to an HTTP/2 peer, causing the peer to build an internal
  queue of responses. Depending on how efficiently this data is
  queued, this can consume excess CPU, memory, or both,
  potentially leading to a denial of service.
  CVE-2019-9513 "Resource Loop": The attacker creates
  multiple request streams and continually shuffles the priority
  of the streams in a way that causes substantial churn to the
  priority tree. This can consume excess CPU, potentially leading
  to a denial of service.
  CVE-2019-9514 "Reset Flood": The attacker opens a number of
  streams and sends an invalid request over each stream that
  should solicit a stream of RST_STREAM frames from the peer.
  Depending on how the peer queues the RST_STREAM frames,
  this can consume excess memory, CPU, or both, potentially
  leading to a denial of service.
  CVE-2019-9515 "Settings Flood": The attacker sends a stream
  of SETTINGS frames to the peer. Since the RFC requires that the
  peer reply with one acknowledgement per SETTINGS frame, an empty
  SETTINGS frame is almost equivalent in behavior to a ping. Depending
  on how efficiently this data is queued, this can consume excess CPU,
  memory, or both, potentially leading to a denial of service.
  CVE-2019-9516 "0-Length Headers Leak": The attacker sends a stream
  of headers with a 0-length header name and 0-length header value,
  optionally Huffman encoded into 1-byte or greater headers. Some
  implementations allocate memory for these headers and keep the
  allocation alive until the session dies. This can consume excess memory,
  potentially leading to a denial of service.
  CVE-2019-9517 "Internal Data Buffering": The attacker opens
  the HTTP/2 window so the peer can send without constraint;
  however, they leave the TCP window closed so the peer cannot
  actually write (many of) the bytes on the wire. The attacker then
  sends a stream of requests for a large response object. Depending
  on how the servers queue the responses, this can consume excess
  memory, CPU, or both, potentially leading to a denial of service.
