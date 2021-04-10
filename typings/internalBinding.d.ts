declare type TypedArray = Uint16Array | Uint32Array | Uint8Array | Uint8ClampedArray | Int16Array | Int32Array | Int8Array | BigInt64Array | Float32Array | Float64Array | BigUint64Array;
declare function InternalBinding(binding: 'types'): {
  isAsyncFunction(value: unknown): value is (...args: unknown[]) => Promise<unknown>,
  isGeneratorFunction(value: unknown): value is GeneratorFunction,
  isAnyArrayBuffer(value: unknown): value is (ArrayBuffer | SharedArrayBuffer),
  isArrayBuffer(value: unknown): value is ArrayBuffer,
  isArgumentsObject(value: unknown): value is ArrayLike<unknown>,
  isBoxedPrimitive(value: unknown): value is (BigInt | Boolean | Number | String | Symbol),
  isDataView(value: unknown): value is DataView,
  isExternal(value: unknown): value is Object,
  isMap(value: unknown): value is Map<unknown, unknown>,
  isMapIterator: (value: unknown) => value is IterableIterator<unknown>,
  isModuleNamespaceObject: (value: unknown) => value is {[Symbol.toStringTag]: 'Module', [key: string]: any},
  isNativeError: (value: unknown) => Error,
  isPromise: (value: unknown) => value is Promise<unknown>,
  isSet: (value: unknown) => value is Set<unknown>,
  isSetIterator: (value: unknown) => value is IterableIterator<unknown>,
  isWeakMap: (value: unknown) => value is WeakMap<object, unknown>,
  isWeakSet: (value: unknown) => value is WeakSet<object>,
  isRegExp: (value: unknown) => RegExp,
  isDate: (value: unknown) => Date,
  isTypedArray: (value: unknown) => value is TypedArray,
  isStringObject: (value: unknown) => value is String,
  isNumberObject: (value: unknown) => value is Number,
  isBooleanObject: (value: unknown) => value is Boolean,
  isBigIntObject: (value: unknown) => value is BigInt,
};
declare function InternalBinding(binding: 'constants'): {
  os: {
    UV_UDP_REUSEADDR: 4,
    dlopen: {
      RTLD_LAZY: 1,
      RTLD_NOW: 2,
      RTLD_GLOBAL: 8,
      RTLD_LOCAL: 4
    },
    errno:{
      E2BIG: 7,
      EACCES: 13,
      EADDRINUSE: 48,
      EADDRNOTAVAIL: 49,
      EAFNOSUPPORT: 47,
      EAGAIN: 35,
      EALREADY: 37,
      EBADF: 9,
      EBADMSG: 94,
      EBUSY: 16,
      ECANCELED: 89,
      ECHILD: 10,
      ECONNABORTED: 53,
      ECONNREFUSED: 61,
      ECONNRESET: 54,
      EDEADLK: 11,
      EDESTADDRREQ: 39,
      EDOM: 33,
      EDQUOT: 69,
      EEXIST: 17,
      EFAULT: 14,
      EFBIG: 27,
      EHOSTUNREACH: 65,
      EIDRM: 90,
      EILSEQ: 92,
      EINPROGRESS: 36,
      EINTR: 4,
      EINVAL: 22,
      EIO: 5,
      EISCONN: 56,
      EISDIR: 21,
      ELOOP: 62,
      EMFILE: 24,
      EMLINK: 31,
      EMSGSIZE: 40,
      EMULTIHOP: 95,
      ENAMETOOLONG: 63,
      ENETDOWN: 50,
      ENETRESET: 52,
      ENETUNREACH: 51,
      ENFILE: 23,
      ENOBUFS: 55,
      ENODATA: 96,
      ENODEV: 19,
      ENOENT: 2,
      ENOEXEC: 8,
      ENOLCK: 77,
      ENOLINK: 97,
      ENOMEM: 12,
      ENOMSG: 91,
      ENOPROTOOPT: 42,
      ENOSPC: 28,
      ENOSR: 98,
      ENOSTR: 99,
      ENOSYS: 78,
      ENOTCONN: 57,
      ENOTDIR: 20,
      ENOTEMPTY: 66,
      ENOTSOCK: 38,
      ENOTSUP: 45,
      ENOTTY: 25,
      ENXIO: 6,
      EOPNOTSUPP: 102,
      EOVERFLOW: 84,
      EPERM: 1,
      EPIPE: 32,
      EPROTO: 100,
      EPROTONOSUPPORT: 43,
      EPROTOTYPE: 41,
      ERANGE: 34,
      EROFS: 30,
      ESPIPE: 29,
      ESRCH: 3,
      ESTALE: 70,
      ETIME: 101,
      ETIMEDOUT: 60,
      ETXTBSY: 26,
      EWOULDBLOCK: 35,
      EXDEV: 18
    },
    signals: {
      SIGHUP: 1,
      SIGINT: 2,
      SIGQUIT: 3,
      SIGILL: 4,
      SIGTRAP: 5,
      SIGABRT: 6,
      SIGIOT: 6,
      SIGBUS: 10,
      SIGFPE: 8,
      SIGKILL: 9,
      SIGUSR1: 30,
      SIGSEGV: 11,
      SIGUSR2: 31,
      SIGPIPE: 13,
      SIGALRM: 14,
      SIGTERM: 15,
      SIGCHLD: 20,
      SIGCONT: 19,
      SIGSTOP: 17,
      SIGTSTP: 18,
      SIGTTIN: 21,
      SIGTTOU: 22,
      SIGURG: 16,
      SIGXCPU: 24,
      SIGXFSZ: 25,
      SIGVTALRM: 26,
      SIGPROF: 27,
      SIGWINCH: 28,
      SIGIO: 23,
      SIGINFO: 29,
      SIGSYS: 12
    },
    priority: {
      PRIORITY_LOW: 19,
      PRIORITY_BELOW_NORMAL: 10,
      PRIORITY_NORMAL: 0,
      PRIORITY_ABOVE_NORMAL: -7,
      PRIORITY_HIGH: -14,
      PRIORITY_HIGHEST: -20
    }
  },
  fs: {
    UV_FS_SYMLINK_DIR: 1,
    UV_FS_SYMLINK_JUNCTION: 2,
    O_RDONLY: 0,
    O_WRONLY: 1,
    O_RDWR: 2,
    UV_DIRENT_UNKNOWN: 0,
    UV_DIRENT_FILE: 1,
    UV_DIRENT_DIR: 2,
    UV_DIRENT_LINK: 3,
    UV_DIRENT_FIFO: 4,
    UV_DIRENT_SOCKET: 5,
    UV_DIRENT_CHAR: 6,
    UV_DIRENT_BLOCK: 7,
    S_IFMT: 61440,
    S_IFREG: 32768,
    S_IFDIR: 16384,
    S_IFCHR: 8192,
    S_IFBLK: 24576,
    S_IFIFO: 4096,
    S_IFLNK: 40960,
    S_IFSOCK: 49152,
    O_CREAT: 512,
    O_EXCL: 2048,
    UV_FS_O_FILEMAP: 0,
    O_NOCTTY: 131072,
    O_TRUNC: 1024,
    O_APPEND: 8,
    O_DIRECTORY: 1048576,
    O_NOFOLLOW: 256,
    O_SYNC: 128,
    O_DSYNC: 4194304,
    O_SYMLINK: 2097152,
    O_NONBLOCK: 4,
    S_IRWXU: 448,
    S_IRUSR: 256,
    S_IWUSR: 128,
    S_IXUSR: 64,
    S_IRWXG: 56,
    S_IRGRP: 32,
    S_IWGRP: 16,
    S_IXGRP: 8,
    S_IRWXO: 7,
    S_IROTH: 4,
    S_IWOTH: 2,
    S_IXOTH: 1,
    F_OK: 0,
    R_OK: 4,
    W_OK: 2,
    X_OK: 1,
    UV_FS_COPYFILE_EXCL: 1,
    COPYFILE_EXCL: 1,
    UV_FS_COPYFILE_FICLONE: 2,
    COPYFILE_FICLONE: 2,
    UV_FS_COPYFILE_FICLONE_FORCE: 4,
    COPYFILE_FICLONE_FORCE: 4
  },
  crypto: {
    OPENSSL_VERSION_NUMBER: 269488319,
    SSL_OP_ALL: 2147485780,
    SSL_OP_ALLOW_NO_DHE_KEX: 1024,
    SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION: 262144,
    SSL_OP_CIPHER_SERVER_PREFERENCE: 4194304,
    SSL_OP_CISCO_ANYCONNECT: 32768,
    SSL_OP_COOKIE_EXCHANGE: 8192,
    SSL_OP_CRYPTOPRO_TLSEXT_BUG: 2147483648,
    SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS: 2048,
    SSL_OP_EPHEMERAL_RSA: 0,
    SSL_OP_LEGACY_SERVER_CONNECT: 4,
    SSL_OP_MICROSOFT_BIG_SSLV3_BUFFER: 0,
    SSL_OP_MICROSOFT_SESS_ID_BUG: 0,
    SSL_OP_MSIE_SSLV2_RSA_PADDING: 0,
    SSL_OP_NETSCAPE_CA_DN_BUG: 0,
    SSL_OP_NETSCAPE_CHALLENGE_BUG: 0,
    SSL_OP_NETSCAPE_DEMO_CIPHER_CHANGE_BUG: 0,
    SSL_OP_NETSCAPE_REUSE_CIPHER_CHANGE_BUG: 0,
    SSL_OP_NO_COMPRESSION: 131072,
    SSL_OP_NO_ENCRYPT_THEN_MAC: 524288,
    SSL_OP_NO_QUERY_MTU: 4096,
    SSL_OP_NO_RENEGOTIATION: 1073741824,
    SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION: 65536,
    SSL_OP_NO_SSLv2: 0,
    SSL_OP_NO_SSLv3: 33554432,
    SSL_OP_NO_TICKET: 16384,
    SSL_OP_NO_TLSv1: 67108864,
    SSL_OP_NO_TLSv1_1: 268435456,
    SSL_OP_NO_TLSv1_2: 134217728,
    SSL_OP_NO_TLSv1_3: 536870912,
    SSL_OP_PKCS1_CHECK_1: 0,
    SSL_OP_PKCS1_CHECK_2: 0,
    SSL_OP_PRIORITIZE_CHACHA: 2097152,
    SSL_OP_SINGLE_DH_USE: 0,
    SSL_OP_SINGLE_ECDH_USE: 0,
    SSL_OP_SSLEAY_080_CLIENT_DH_BUG: 0,
    SSL_OP_SSLREF2_REUSE_CERT_TYPE_BUG: 0,
    SSL_OP_TLS_BLOCK_PADDING_BUG: 0,
    SSL_OP_TLS_D5_BUG: 0,
    SSL_OP_TLS_ROLLBACK_BUG: 8388608,
    ENGINE_METHOD_RSA: 1,
    ENGINE_METHOD_DSA: 2,
    ENGINE_METHOD_DH: 4,
    ENGINE_METHOD_RAND: 8,
    ENGINE_METHOD_EC: 2048,
    ENGINE_METHOD_CIPHERS: 64,
    ENGINE_METHOD_DIGESTS: 128,
    ENGINE_METHOD_PKEY_METHS: 512,
    ENGINE_METHOD_PKEY_ASN1_METHS: 1024,
    ENGINE_METHOD_ALL: 65535,
    ENGINE_METHOD_NONE: 0,
    DH_CHECK_P_NOT_SAFE_PRIME: 2,
    DH_CHECK_P_NOT_PRIME: 1,
    DH_UNABLE_TO_CHECK_GENERATOR: 4,
    DH_NOT_SUITABLE_GENERATOR: 8,
    ALPN_ENABLED: 1,
    RSA_PKCS1_PADDING: 1,
    RSA_SSLV23_PADDING: 2,
    RSA_NO_PADDING: 3,
    RSA_PKCS1_OAEP_PADDING: 4,
    RSA_X931_PADDING: 5,
    RSA_PKCS1_PSS_PADDING: 6,
    RSA_PSS_SALTLEN_DIGEST: -1,
    RSA_PSS_SALTLEN_MAX_SIGN: -2,
    RSA_PSS_SALTLEN_AUTO: -2,
    defaultCoreCipherList: 'TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256:TLS_AES_128_GCM_SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-SHA256:DHE-RSA-AES128-SHA256:ECDHE-RSA-AES256-SHA384:DHE-RSA-AES256-SHA384:ECDHE-RSA-AES256-SHA256:DHE-RSA-AES256-SHA256:HIGH:!aNULL:!eNULL:!EXPORT:!DES:!RC4:!MD5:!PSK:!SRP:!CAMELLIA',
    TLS1_VERSION: 769,
    TLS1_1_VERSION: 770,
    TLS1_2_VERSION: 771,
    TLS1_3_VERSION: 772,
    POINT_CONVERSION_COMPRESSED: 2,
    POINT_CONVERSION_UNCOMPRESSED: 4,
    POINT_CONVERSION_HYBRID: 6
  },
  zlib: {
    Z_NO_FLUSH: 0,
    Z_PARTIAL_FLUSH: 1,
    Z_SYNC_FLUSH: 2,
    Z_FULL_FLUSH: 3,
    Z_FINISH: 4,
    Z_BLOCK: 5,
    Z_OK: 0,
    Z_STREAM_END: 1,
    Z_NEED_DICT: 2,
    Z_ERRNO: -1,
    Z_STREAM_ERROR: -2,
    Z_DATA_ERROR: -3,
    Z_MEM_ERROR: -4,
    Z_BUF_ERROR: -5,
    Z_VERSION_ERROR: -6,
    Z_NO_COMPRESSION: 0,
    Z_BEST_SPEED: 1,
    Z_BEST_COMPRESSION: 9,
    Z_DEFAULT_COMPRESSION: -1,
    Z_FILTERED: 1,
    Z_HUFFMAN_ONLY: 2,
    Z_RLE: 3,
    Z_FIXED: 4,
    Z_DEFAULT_STRATEGY: 0,
    ZLIB_VERNUM: 4784,
    DEFLATE: 1,
    INFLATE: 2,
    GZIP: 3,
    GUNZIP: 4,
    DEFLATERAW: 5,
    INFLATERAW: 6,
    UNZIP: 7,
    BROTLI_DECODE: 8,
    BROTLI_ENCODE: 9,
    Z_MIN_WINDOWBITS: 8,
    Z_MAX_WINDOWBITS: 15,
    Z_DEFAULT_WINDOWBITS: 15,
    Z_MIN_CHUNK: 64,
    Z_MAX_CHUNK: number,
    Z_DEFAULT_CHUNK: 16384,
    Z_MIN_MEMLEVEL: 1,
    Z_MAX_MEMLEVEL: 9,
    Z_DEFAULT_MEMLEVEL: 8,
    Z_MIN_LEVEL: -1,
    Z_MAX_LEVEL: 9,
    Z_DEFAULT_LEVEL: -1,
    BROTLI_OPERATION_PROCESS: 0,
    BROTLI_OPERATION_FLUSH: 1,
    BROTLI_OPERATION_FINISH: 2,
    BROTLI_OPERATION_EMIT_METADATA: 3,
    BROTLI_PARAM_MODE: 0,
    BROTLI_MODE_GENERIC: 0,
    BROTLI_MODE_TEXT: 1,
    BROTLI_MODE_FONT: 2,
    BROTLI_DEFAULT_MODE: 0,
    BROTLI_PARAM_QUALITY: 1,
    BROTLI_MIN_QUALITY: 0,
    BROTLI_MAX_QUALITY: 11,
    BROTLI_DEFAULT_QUALITY: 11,
    BROTLI_PARAM_LGWIN: 2,
    BROTLI_MIN_WINDOW_BITS: 10,
    BROTLI_MAX_WINDOW_BITS: 24,
    BROTLI_LARGE_MAX_WINDOW_BITS: 30,
    BROTLI_DEFAULT_WINDOW: 22,
    BROTLI_PARAM_LGBLOCK: 3,
    BROTLI_MIN_INPUT_BLOCK_BITS: 16,
    BROTLI_MAX_INPUT_BLOCK_BITS: 24,
    BROTLI_PARAM_DISABLE_LITERAL_CONTEXT_MODELING: 4,
    BROTLI_PARAM_SIZE_HINT: 5,
    BROTLI_PARAM_LARGE_WINDOW: 6,
    BROTLI_PARAM_NPOSTFIX: 7,
    BROTLI_PARAM_NDIRECT: 8,
    BROTLI_DECODER_RESULT_ERROR: 0,
    BROTLI_DECODER_RESULT_SUCCESS: 1,
    BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT: 2,
    BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT: 3,
    BROTLI_DECODER_PARAM_DISABLE_RING_BUFFER_REALLOCATION: 0,
    BROTLI_DECODER_PARAM_LARGE_WINDOW: 1,
    BROTLI_DECODER_NO_ERROR: 0,
    BROTLI_DECODER_SUCCESS: 1,
    BROTLI_DECODER_NEEDS_MORE_INPUT: 2,
    BROTLI_DECODER_NEEDS_MORE_OUTPUT: 3,
    BROTLI_DECODER_ERROR_FORMAT_EXUBERANT_NIBBLE: -1,
    BROTLI_DECODER_ERROR_FORMAT_RESERVED: -2,
    BROTLI_DECODER_ERROR_FORMAT_EXUBERANT_META_NIBBLE: -3,
    BROTLI_DECODER_ERROR_FORMAT_SIMPLE_HUFFMAN_ALPHABET: -4,
    BROTLI_DECODER_ERROR_FORMAT_SIMPLE_HUFFMAN_SAME: -5,
    BROTLI_DECODER_ERROR_FORMAT_CL_SPACE: -6,
    BROTLI_DECODER_ERROR_FORMAT_HUFFMAN_SPACE: -7,
    BROTLI_DECODER_ERROR_FORMAT_CONTEXT_MAP_REPEAT: -8,
    BROTLI_DECODER_ERROR_FORMAT_BLOCK_LENGTH_1: -9,
    BROTLI_DECODER_ERROR_FORMAT_BLOCK_LENGTH_2: -10,
    BROTLI_DECODER_ERROR_FORMAT_TRANSFORM: -11,
    BROTLI_DECODER_ERROR_FORMAT_DICTIONARY: -12,
    BROTLI_DECODER_ERROR_FORMAT_WINDOW_BITS: -13,
    BROTLI_DECODER_ERROR_FORMAT_PADDING_1: -14,
    BROTLI_DECODER_ERROR_FORMAT_PADDING_2: -15,
    BROTLI_DECODER_ERROR_FORMAT_DISTANCE: -16,
    BROTLI_DECODER_ERROR_DICTIONARY_NOT_SET: -19,
    BROTLI_DECODER_ERROR_INVALID_ARGUMENTS: -20,
    BROTLI_DECODER_ERROR_ALLOC_CONTEXT_MODES: -21,
    BROTLI_DECODER_ERROR_ALLOC_TREE_GROUPS: -22,
    BROTLI_DECODER_ERROR_ALLOC_CONTEXT_MAP: -25,
    BROTLI_DECODER_ERROR_ALLOC_RING_BUFFER_1: -26,
    BROTLI_DECODER_ERROR_ALLOC_RING_BUFFER_2: -27,
    BROTLI_DECODER_ERROR_ALLOC_BLOCK_TYPE_TREES: -30,
    BROTLI_DECODER_ERROR_UNREACHABLE: -31
  },
  trace: {
    TRACE_EVENT_PHASE_BEGIN: 66,
    TRACE_EVENT_PHASE_END: 69,
    TRACE_EVENT_PHASE_COMPLETE: 88,
    TRACE_EVENT_PHASE_INSTANT: 73,
    TRACE_EVENT_PHASE_ASYNC_BEGIN: 83,
    TRACE_EVENT_PHASE_ASYNC_STEP_INTO: 84,
    TRACE_EVENT_PHASE_ASYNC_STEP_PAST: 112,
    TRACE_EVENT_PHASE_ASYNC_END: 70,
    TRACE_EVENT_PHASE_NESTABLE_ASYNC_BEGIN: 98,
    TRACE_EVENT_PHASE_NESTABLE_ASYNC_END: 101,
    TRACE_EVENT_PHASE_NESTABLE_ASYNC_INSTANT: 110,
    TRACE_EVENT_PHASE_FLOW_BEGIN: 115,
    TRACE_EVENT_PHASE_FLOW_STEP: 116,
    TRACE_EVENT_PHASE_FLOW_END: 102,
    TRACE_EVENT_PHASE_METADATA: 77,
    TRACE_EVENT_PHASE_COUNTER: 67,
    TRACE_EVENT_PHASE_SAMPLE: 80,
    TRACE_EVENT_PHASE_CREATE_OBJECT: 78,
    TRACE_EVENT_PHASE_SNAPSHOT_OBJECT: 79,
    TRACE_EVENT_PHASE_DELETE_OBJECT: 68,
    TRACE_EVENT_PHASE_MEMORY_DUMP: 118,
    TRACE_EVENT_PHASE_MARK: 82,
    TRACE_EVENT_PHASE_CLOCK_SYNC: 99,
    TRACE_EVENT_PHASE_ENTER_CONTEXT: 40,
    TRACE_EVENT_PHASE_LEAVE_CONTEXT: 41,
    TRACE_EVENT_PHASE_LINK_IDS: 61
  }
};
declare function InternalBinding(binding: 'config'): {
  isDebugBuild: boolean,
  hasOpenSSL: boolean,
  fipsMode: boolean,
  hasIntl: boolean,
  hasTracing: boolean,
  hasNodeOptions: boolean,
  hasInspector: boolean,
  noBrowserGlobals: boolean,
  bits: number,
  hasDtrace: boolean
}

declare namespace InternalFSBinding {
  class FSReqCallback<ResultType = unknown> {
    constructor(bigint?: boolean);
    oncomplete: ((error: Error) => void) | ((error: null, result: ResultType) => void);
    context: any;
  }

  interface FSSyncContext {
    fd?: number;
    path?: string;
    dest?: string;
    errno?: string;
    message?: string;
    syscall?: string;
    error?: Error;
  }

  type Buffer = Uint8Array;
  type StringOrBuffer = string | Buffer;

  const kUsePromises: symbol;

  class FileHandle {
    constructor(fd: number, offset: number, length: number);
    fd: number;
    getAsyncId(): number;
    close(): Promise<void>;
    onread: () => void;
    stream: unknown;
  }

  class StatWatcher {
    constructor(useBigint: boolean);
    initialized: boolean;
    start(path: string, interval: number): number;
    getAsyncId(): number;
    close(): void;
    ref(): void;
    unref(): void;
    onchange: (status: number, eventType: string, filename: string | Buffer) => void;
  }

  function access(path: StringOrBuffer, mode: number, req: FSReqCallback): void;
  function access(path: StringOrBuffer, mode: number, req: undefined, ctx: FSSyncContext): void;
  function access(path: StringOrBuffer, mode: number, usePromises: typeof kUsePromises): Promise<void>;

  function chmod(path: string, mode: number, req: FSReqCallback): void;
  function chmod(path: string, mode: number, req: undefined, ctx: FSSyncContext): void;
  function chmod(path: string, mode: number, usePromises: typeof kUsePromises): Promise<void>;

  function chown(path: string, uid: number, gid: number, req: FSReqCallback): void;
  function chown(path: string, uid: number, gid: number, req: undefined, ctx: FSSyncContext): void;
  function chown(path: string, uid: number, gid: number, usePromises: typeof kUsePromises): Promise<void>;

  function close(fd: number, req: FSReqCallback): void;
  function close(fd: number, req: undefined, ctx: FSSyncContext): void;

  function copyFile(src: StringOrBuffer, dest: StringOrBuffer, mode: number, req: FSReqCallback): void;
  function copyFile(src: StringOrBuffer, dest: StringOrBuffer, mode: number, req: undefined, ctx: FSSyncContext): void;
  function copyFile(src: StringOrBuffer, dest: StringOrBuffer, mode: number, usePromises: typeof kUsePromises): Promise<void>;

  function fchmod(fd: number, mode: number, req: FSReqCallback): void;
  function fchmod(fd: number, mode: number, req: undefined, ctx: FSSyncContext): void;
  function fchmod(fd: number, mode: number, usePromises: typeof kUsePromises): Promise<void>;

  function fchown(fd: number, uid: number, gid: number, req: FSReqCallback): void;
  function fchown(fd: number, uid: number, gid: number, req: undefined, ctx: FSSyncContext): void;
  function fchown(fd: number, uid: number, gid: number, usePromises: typeof kUsePromises): Promise<void>;

  function fdatasync(fd: number, req: FSReqCallback): void;
  function fdatasync(fd: number, req: undefined, ctx: FSSyncContext): void;
  function fdatasync(fd: number, usePromises: typeof kUsePromises): Promise<void>;

  function fstat(fd: number, useBigint: boolean, req: FSReqCallback<Float64Array | BigUint64Array>): void;
  function fstat(fd: number, useBigint: true, req: FSReqCallback<BigUint64Array>): void;
  function fstat(fd: number, useBigint: false, req: FSReqCallback<Float64Array>): void;
  function fstat(fd: number, useBigint: boolean, req: undefined, ctx: FSSyncContext): Float64Array | BigUint64Array;
  function fstat(fd: number, useBigint: true, req: undefined, ctx: FSSyncContext): BigUint64Array;
  function fstat(fd: number, useBigint: false, req: undefined, ctx: FSSyncContext): Float64Array;
  function fstat(fd: number, useBigint: boolean, usePromises: typeof kUsePromises): Promise<Float64Array | BigUint64Array>;
  function fstat(fd: number, useBigint: true, usePromises: typeof kUsePromises): Promise<BigUint64Array>;
  function fstat(fd: number, useBigint: false, usePromises: typeof kUsePromises): Promise<Float64Array>;

  function fsync(fd: number, req: FSReqCallback): void;
  function fsync(fd: number, req: undefined, ctx: FSSyncContext): void;
  function fsync(fd: number, usePromises: typeof kUsePromises): Promise<void>;

  function ftruncate(fd: number, len: number, req: FSReqCallback): void;
  function ftruncate(fd: number, len: number, req: undefined, ctx: FSSyncContext): void;
  function ftruncate(fd: number, len: number, usePromises: typeof kUsePromises): Promise<void>;

  function futimes(fd: number, atime: number, mtime: number, req: FSReqCallback): void;
  function futimes(fd: number, atime: number, mtime: number, req: undefined, ctx: FSSyncContext): void;
  function futimes(fd: number, atime: number, mtime: number, usePromises: typeof kUsePromises): Promise<void>;

  function internalModuleReadJSON(path: string): [] | [string, boolean];
  function internalModuleStat(path: string): number;
  
  function lchown(path: string, uid: number, gid: number, req: FSReqCallback): void;
  function lchown(path: string, uid: number, gid: number, req: undefined, ctx: FSSyncContext): void;
  function lchown(path: string, uid: number, gid: number, usePromises: typeof kUsePromises): Promise<void>;

  function link(existingPath: string, newPath: string, req: FSReqCallback): void;
  function link(existingPath: string, newPath: string, req: undefined, ctx: FSSyncContext): void;
  function link(existingPath: string, newPath: string, usePromises: typeof kUsePromises): Promise<void>;

  function lstat(path: StringOrBuffer, useBigint: boolean, req: FSReqCallback<Float64Array | BigUint64Array>): void;
  function lstat(path: StringOrBuffer, useBigint: true, req: FSReqCallback<BigUint64Array>): void;
  function lstat(path: StringOrBuffer, useBigint: false, req: FSReqCallback<Float64Array>): void;
  function lstat(path: StringOrBuffer, useBigint: boolean, req: undefined, ctx: FSSyncContext): Float64Array | BigUint64Array;
  function lstat(path: StringOrBuffer, useBigint: true, req: undefined, ctx: FSSyncContext): BigUint64Array;
  function lstat(path: StringOrBuffer, useBigint: false, req: undefined, ctx: FSSyncContext): Float64Array;
  function lstat(path: StringOrBuffer, useBigint: boolean, usePromises: typeof kUsePromises): Promise<Float64Array | BigUint64Array>;
  function lstat(path: StringOrBuffer, useBigint: true, usePromises: typeof kUsePromises): Promise<BigUint64Array>;
  function lstat(path: StringOrBuffer, useBigint: false, usePromises: typeof kUsePromises): Promise<Float64Array>;

  function lutimes(path: string, atime: number, mtime: number, req: FSReqCallback): void;
  function lutimes(path: string, atime: number, mtime: number, req: undefined, ctx: FSSyncContext): void;
  function lutimes(path: string, atime: number, mtime: number, usePromises: typeof kUsePromises): Promise<void>;

  function mkdtemp(prefix: string, encoding: unknown, req: FSReqCallback<string>): void;
  function mkdtemp(prefix: string, encoding: unknown, req: undefined, ctx: FSSyncContext): string;
  function mkdtemp(prefix: string, encoding: unknown, usePromises: typeof kUsePromises): Promise<string>;

  function mkdir(path: string, mode: number, recursive: boolean, req: FSReqCallback<void | string>): void;
  function mkdir(path: string, mode: number, recursive: true, req: FSReqCallback<string>): void;
  function mkdir(path: string, mode: number, recursive: false, req: FSReqCallback<void>): void;
  function mkdir(path: string, mode: number, recursive: boolean, req: undefined, ctx: FSSyncContext): void | string;
  function mkdir(path: string, mode: number, recursive: true, req: undefined, ctx: FSSyncContext): string;
  function mkdir(path: string, mode: number, recursive: false, req: undefined, ctx: FSSyncContext): void;
  function mkdir(path: string, mode: number, recursive: boolean, usePromises: typeof kUsePromises): Promise<void | string>;
  function mkdir(path: string, mode: number, recursive: true, usePromises: typeof kUsePromises): Promise<string>;
  function mkdir(path: string, mode: number, recursive: false, usePromises: typeof kUsePromises): Promise<void>;

  function open(path: StringOrBuffer, flags: number, mode: number, req: FSReqCallback<number>): void;
  function open(path: StringOrBuffer, flags: number, mode: number, req: undefined, ctx: FSSyncContext): number;

  function openFileHandle(path: StringOrBuffer, flags: number, mode: number, usePromises: typeof kUsePromises): Promise<FileHandle>;

  function read(fd: number, buffer: ArrayBufferView, offset: number, length: number, position: number, req: FSReqCallback<number>): void;
  function read(fd: number, buffer: ArrayBufferView, offset: number, length: number, position: number, req: undefined, ctx: FSSyncContext): number;
  function read(fd: number, buffer: ArrayBufferView, offset: number, length: number, position: number, usePromises: typeof kUsePromises): Promise<number>;

  function readBuffers(fd: number, buffers: ArrayBufferView[], position: number, req: FSReqCallback<number>): void;
  function readBuffers(fd: number, buffers: ArrayBufferView[], position: number, req: undefined, ctx: FSSyncContext): number;
  function readBuffers(fd: number, buffers: ArrayBufferView[], position: number, usePromises: typeof kUsePromises): Promise<number>;

  function readdir(path: StringOrBuffer, encoding: unknown, withFileTypes: boolean, req: FSReqCallback<string[] | [string[], number[]]>): void;
  function readdir(path: StringOrBuffer, encoding: unknown, withFileTypes: true, req: FSReqCallback<[string[], number[]]>): void;
  function readdir(path: StringOrBuffer, encoding: unknown, withFileTypes: false, req: FSReqCallback<string[]>): void;
  function readdir(path: StringOrBuffer, encoding: unknown, withFileTypes: boolean, req: undefined, ctx: FSSyncContext): string[] | [string[], number[]];
  function readdir(path: StringOrBuffer, encoding: unknown, withFileTypes: true, req: undefined, ctx: FSSyncContext): [string[], number[]];
  function readdir(path: StringOrBuffer, encoding: unknown, withFileTypes: false, req: undefined, ctx: FSSyncContext): string[];
  function readdir(path: StringOrBuffer, encoding: unknown, withFileTypes: boolean, usePromises: typeof kUsePromises): Promise<string[] | [string[], number[]]>;
  function readdir(path: StringOrBuffer, encoding: unknown, withFileTypes: true, usePromises: typeof kUsePromises): Promise<[string[], number[]]>;
  function readdir(path: StringOrBuffer, encoding: unknown, withFileTypes: false, usePromises: typeof kUsePromises): Promise<string[]>;

  function readlink(path: StringOrBuffer, encoding: unknown, req: FSReqCallback<string | Buffer>): void;
  function readlink(path: StringOrBuffer, encoding: unknown, req: undefined, ctx: FSSyncContext): string | Buffer;
  function readlink(path: StringOrBuffer, encoding: unknown, usePromises: typeof kUsePromises): Promise<string | Buffer>;

  function realpath(path: StringOrBuffer, encoding: unknown, req: FSReqCallback<string | Buffer>): void;
  function realpath(path: StringOrBuffer, encoding: unknown, req: undefined, ctx: FSSyncContext): string | Buffer;
  function realpath(path: StringOrBuffer, encoding: unknown, usePromises: typeof kUsePromises): Promise<string | Buffer>;

  function rename(oldPath: string, newPath: string, req: FSReqCallback): void;
  function rename(oldPath: string, newPath: string, req: undefined, ctx: FSSyncContext): void;
  function rename(oldPath: string, newPath: string, usePromises: typeof kUsePromises): Promise<void>;

  function rmdir(path: string, req: FSReqCallback): void;
  function rmdir(path: string, req: undefined, ctx: FSSyncContext): void;
  function rmdir(path: string, usePromises: typeof kUsePromises): Promise<void>;

  function stat(path: StringOrBuffer, useBigint: boolean, req: FSReqCallback<Float64Array | BigUint64Array>): void;
  function stat(path: StringOrBuffer, useBigint: true, req: FSReqCallback<BigUint64Array>): void;
  function stat(path: StringOrBuffer, useBigint: false, req: FSReqCallback<Float64Array>): void;
  function stat(path: StringOrBuffer, useBigint: boolean, req: undefined, ctx: FSSyncContext): Float64Array | BigUint64Array;
  function stat(path: StringOrBuffer, useBigint: true, req: undefined, ctx: FSSyncContext): BigUint64Array;
  function stat(path: StringOrBuffer, useBigint: false, req: undefined, ctx: FSSyncContext): Float64Array;
  function stat(path: StringOrBuffer, useBigint: boolean, usePromises: typeof kUsePromises): Promise<Float64Array | BigUint64Array>;
  function stat(path: StringOrBuffer, useBigint: true, usePromises: typeof kUsePromises): Promise<BigUint64Array>;
  function stat(path: StringOrBuffer, useBigint: false, usePromises: typeof kUsePromises): Promise<Float64Array>;

  function symlink(target: StringOrBuffer, path: StringOrBuffer, type: number, req: FSReqCallback): void;
  function symlink(target: StringOrBuffer, path: StringOrBuffer, type: number, req: undefined, ctx: FSSyncContext): void;
  function symlink(target: StringOrBuffer, path: StringOrBuffer, type: number, usePromises: typeof kUsePromises): Promise<void>;
  
  function unlink(path: string, req: FSReqCallback): void;
  function unlink(path: string, req: undefined, ctx: FSSyncContext): void;
  function unlink(path: string, usePromises: typeof kUsePromises): Promise<void>;

  function utimes(path: string, atime: number, mtime: number, req: FSReqCallback): void;
  function utimes(path: string, atime: number, mtime: number, req: undefined, ctx: FSSyncContext): void;
  function utimes(path: string, atime: number, mtime: number, usePromises: typeof kUsePromises): Promise<void>;

  function writeBuffer(fd: number, buffer: ArrayBufferView, offset: number, length: number, position: number | null, req: FSReqCallback<number>): void;
  function writeBuffer(fd: number, buffer: ArrayBufferView, offset: number, length: number, position: number | null, req: undefined, ctx: FSSyncContext): number;
  function writeBuffer(fd: number, buffer: ArrayBufferView, offset: number, length: number, position: number | null, usePromises: typeof kUsePromises): Promise<number>;

  function writeBuffers(fd: number, buffers: ArrayBufferView[], position: number, req: FSReqCallback<number>): void;
  function writeBuffers(fd: number, buffers: ArrayBufferView[], position: number, req: undefined, ctx: FSSyncContext): number;
  function writeBuffers(fd: number, buffers: ArrayBufferView[], position: number, usePromises: typeof kUsePromises): Promise<number>;

  function writeString(fd: number, value: string, pos: unknown, encoding: unknown, req: FSReqCallback<number>): void;
  function writeString(fd: number, value: string, pos: unknown, encoding: unknown, req: undefined, ctx: FSSyncContext): number;
  function writeString(fd: number, value: string, pos: unknown, encoding: unknown, usePromises: typeof kUsePromises): Promise<number>;
}

declare function InternalBinding(binding: 'fs'): {
  FSReqCallback: typeof InternalFSBinding.FSReqCallback;

  FileHandle: typeof InternalFSBinding.FileHandle;

  kUsePromises: typeof InternalFSBinding.kUsePromises;

  statValues: Float64Array;
  bigintStatValues: BigUint64Array;

  kFsStatsFieldsNumber: number;
  StatWatcher: typeof InternalFSBinding.StatWatcher;

  access: typeof InternalFSBinding.access;
  chmod: typeof InternalFSBinding.chmod;
  chown: typeof InternalFSBinding.chown;
  close: typeof InternalFSBinding.close;
  copyFile: typeof InternalFSBinding.copyFile;
  fchmod: typeof InternalFSBinding.fchmod;
  fchown: typeof InternalFSBinding.fchown;
  fdatasync: typeof InternalFSBinding.fdatasync;
  fstat: typeof InternalFSBinding.fstat;
  fsync: typeof InternalFSBinding.fsync;
  ftruncate: typeof InternalFSBinding.ftruncate;
  futimes: typeof InternalFSBinding.futimes;
  internalModuleReadJSON: typeof InternalFSBinding.internalModuleReadJSON;
  internalModuleStat: typeof InternalFSBinding.internalModuleStat;
  lchown: typeof InternalFSBinding.lchown;
  link: typeof InternalFSBinding.link;
  lstat: typeof InternalFSBinding.lstat;
  lutimes: typeof InternalFSBinding.lutimes;
  mkdtemp: typeof InternalFSBinding.mkdtemp;
  mkdir: typeof InternalFSBinding.mkdir;
  open: typeof InternalFSBinding.open;
  openFileHandle: typeof InternalFSBinding.openFileHandle;
  read: typeof InternalFSBinding.read;
  readBuffers: typeof InternalFSBinding.readBuffers;
  readdir: typeof InternalFSBinding.readdir;
  readlink: typeof InternalFSBinding.readlink;
  realpath: typeof InternalFSBinding.realpath;
  rename: typeof InternalFSBinding.rename;
  rmdir: typeof InternalFSBinding.rmdir;
  stat: typeof InternalFSBinding.stat;
  symlink: typeof InternalFSBinding.symlink;
  unlink: typeof InternalFSBinding.unlink;
  utimes: typeof InternalFSBinding.utimes;
  writeBuffer: typeof InternalFSBinding.writeBuffer;
  writeBuffers: typeof InternalFSBinding.writeBuffers;
  writeString: typeof InternalFSBinding.writeString;
};
