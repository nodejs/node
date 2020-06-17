'use strict';

const {
  ArrayIsArray,
  Error,
  MathMax,
  Number,
  ObjectCreate,
  ObjectKeys,
  Set,
  StringPrototypeToLowerCase,
  Symbol,
} = primordials;

const {
  kUpdateTimer,
  kHandle,
} = require('internal/stream_base_commons');

const binding = internalBinding('http2');
const {
  codes: {
    ERR_HTTP2_HEADER_SINGLE_VALUE,
    ERR_HTTP2_INVALID_CONNECTION_HEADERS,
    ERR_HTTP2_INVALID_PSEUDOHEADER,
    ERR_HTTP2_INVALID_SETTING_VALUE,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_HTTP_TOKEN,
    ERR_INVALID_OPT_VALUE,
  },
  addCodeToName,
  hideStackFrames
} = require('internal/errors');

const kSensitiveHeaders = Symbol('nodejs.http2.sensitiveHeaders');
const kSocket = Symbol('socket');
const kState = Symbol('state');
const kProxySocket = Symbol('proxySocket');
const kRequest = Symbol('request');

const {
  NGHTTP2_NV_FLAG_NONE,
  NGHTTP2_NV_FLAG_NO_INDEX,
  NGHTTP2_DEFAULT_WEIGHT,
  NGHTTP2_SESSION_CLIENT,
  NGHTTP2_SESSION_SERVER,

  HTTP2_HEADER_STATUS,
  HTTP2_HEADER_METHOD,
  HTTP2_HEADER_AUTHORITY,
  HTTP2_HEADER_SCHEME,
  HTTP2_HEADER_PATH,
  HTTP2_HEADER_PROTOCOL,
  HTTP2_HEADER_ACCESS_CONTROL_ALLOW_CREDENTIALS,
  HTTP2_HEADER_ACCESS_CONTROL_MAX_AGE,
  HTTP2_HEADER_ACCESS_CONTROL_REQUEST_METHOD,
  HTTP2_HEADER_AGE,
  HTTP2_HEADER_AUTHORIZATION,
  HTTP2_HEADER_CONTENT_ENCODING,
  HTTP2_HEADER_CONTENT_LANGUAGE,
  HTTP2_HEADER_CONTENT_LENGTH,
  HTTP2_HEADER_CONTENT_LOCATION,
  HTTP2_HEADER_CONTENT_MD5,
  HTTP2_HEADER_CONTENT_RANGE,
  HTTP2_HEADER_CONTENT_TYPE,
  HTTP2_HEADER_COOKIE,
  HTTP2_HEADER_DATE,
  HTTP2_HEADER_DNT,
  HTTP2_HEADER_ETAG,
  HTTP2_HEADER_EXPIRES,
  HTTP2_HEADER_FROM,
  HTTP2_HEADER_IF_MATCH,
  HTTP2_HEADER_IF_NONE_MATCH,
  HTTP2_HEADER_IF_MODIFIED_SINCE,
  HTTP2_HEADER_IF_RANGE,
  HTTP2_HEADER_IF_UNMODIFIED_SINCE,
  HTTP2_HEADER_LAST_MODIFIED,
  HTTP2_HEADER_LOCATION,
  HTTP2_HEADER_MAX_FORWARDS,
  HTTP2_HEADER_PROXY_AUTHORIZATION,
  HTTP2_HEADER_RANGE,
  HTTP2_HEADER_REFERER,
  HTTP2_HEADER_RETRY_AFTER,
  HTTP2_HEADER_SET_COOKIE,
  HTTP2_HEADER_TK,
  HTTP2_HEADER_UPGRADE_INSECURE_REQUESTS,
  HTTP2_HEADER_USER_AGENT,
  HTTP2_HEADER_X_CONTENT_TYPE_OPTIONS,

  HTTP2_HEADER_CONNECTION,
  HTTP2_HEADER_UPGRADE,
  HTTP2_HEADER_HTTP2_SETTINGS,
  HTTP2_HEADER_TE,
  HTTP2_HEADER_TRANSFER_ENCODING,
  HTTP2_HEADER_HOST,
  HTTP2_HEADER_KEEP_ALIVE,
  HTTP2_HEADER_PROXY_CONNECTION,

  HTTP2_METHOD_DELETE,
  HTTP2_METHOD_GET,
  HTTP2_METHOD_HEAD
} = binding.constants;

// This set is defined strictly by the HTTP/2 specification. Only
// :-prefixed headers defined by that specification may be added to
// this set.
const kValidPseudoHeaders = new Set([
  HTTP2_HEADER_STATUS,
  HTTP2_HEADER_METHOD,
  HTTP2_HEADER_AUTHORITY,
  HTTP2_HEADER_SCHEME,
  HTTP2_HEADER_PATH,
  HTTP2_HEADER_PROTOCOL
]);

// This set contains headers that are permitted to have only a single
// value. Multiple instances must not be specified.
const kSingleValueHeaders = new Set([
  HTTP2_HEADER_STATUS,
  HTTP2_HEADER_METHOD,
  HTTP2_HEADER_AUTHORITY,
  HTTP2_HEADER_SCHEME,
  HTTP2_HEADER_PATH,
  HTTP2_HEADER_PROTOCOL,
  HTTP2_HEADER_ACCESS_CONTROL_ALLOW_CREDENTIALS,
  HTTP2_HEADER_ACCESS_CONTROL_MAX_AGE,
  HTTP2_HEADER_ACCESS_CONTROL_REQUEST_METHOD,
  HTTP2_HEADER_AGE,
  HTTP2_HEADER_AUTHORIZATION,
  HTTP2_HEADER_CONTENT_ENCODING,
  HTTP2_HEADER_CONTENT_LANGUAGE,
  HTTP2_HEADER_CONTENT_LENGTH,
  HTTP2_HEADER_CONTENT_LOCATION,
  HTTP2_HEADER_CONTENT_MD5,
  HTTP2_HEADER_CONTENT_RANGE,
  HTTP2_HEADER_CONTENT_TYPE,
  HTTP2_HEADER_DATE,
  HTTP2_HEADER_DNT,
  HTTP2_HEADER_ETAG,
  HTTP2_HEADER_EXPIRES,
  HTTP2_HEADER_FROM,
  HTTP2_HEADER_IF_MATCH,
  HTTP2_HEADER_IF_MODIFIED_SINCE,
  HTTP2_HEADER_IF_NONE_MATCH,
  HTTP2_HEADER_IF_RANGE,
  HTTP2_HEADER_IF_UNMODIFIED_SINCE,
  HTTP2_HEADER_LAST_MODIFIED,
  HTTP2_HEADER_LOCATION,
  HTTP2_HEADER_MAX_FORWARDS,
  HTTP2_HEADER_PROXY_AUTHORIZATION,
  HTTP2_HEADER_RANGE,
  HTTP2_HEADER_REFERER,
  HTTP2_HEADER_RETRY_AFTER,
  HTTP2_HEADER_TK,
  HTTP2_HEADER_UPGRADE_INSECURE_REQUESTS,
  HTTP2_HEADER_USER_AGENT,
  HTTP2_HEADER_X_CONTENT_TYPE_OPTIONS
]);

// The HTTP methods in this set are specifically defined as assigning no
// meaning to the request payload. By default, unless the user explicitly
// overrides the endStream option on the request method, the endStream
// option will be defaulted to true when these methods are used.
const kNoPayloadMethods = new Set([
  HTTP2_METHOD_DELETE,
  HTTP2_METHOD_GET,
  HTTP2_METHOD_HEAD
]);

// The following ArrayBuffer instances are used to share memory more efficiently
// with the native binding side for a number of methods. These are not intended
// to be used directly by users in any way. The ArrayBuffers are created on
// the native side with values that are filled in on demand, the js code then
// reads those values out. The set of IDX constants that follow identify the
// relevant data positions within these buffers.
const { settingsBuffer, optionsBuffer } = binding;

// Note that Float64Array is used here because there is no Int64Array available
// and these deal with numbers that can be beyond the range of Uint32 and Int32.
// The values set on the native side will always be integers. This is not a
// unique example of this, this pattern can be found in use in other parts of
// Node.js core as a performance optimization.
const { sessionState, streamState } = binding;

const IDX_SETTINGS_HEADER_TABLE_SIZE = 0;
const IDX_SETTINGS_ENABLE_PUSH = 1;
const IDX_SETTINGS_INITIAL_WINDOW_SIZE = 2;
const IDX_SETTINGS_MAX_FRAME_SIZE = 3;
const IDX_SETTINGS_MAX_CONCURRENT_STREAMS = 4;
const IDX_SETTINGS_MAX_HEADER_LIST_SIZE = 5;
const IDX_SETTINGS_ENABLE_CONNECT_PROTOCOL = 6;
const IDX_SETTINGS_FLAGS = 7;

const IDX_SESSION_STATE_EFFECTIVE_LOCAL_WINDOW_SIZE = 0;
const IDX_SESSION_STATE_EFFECTIVE_RECV_DATA_LENGTH = 1;
const IDX_SESSION_STATE_NEXT_STREAM_ID = 2;
const IDX_SESSION_STATE_LOCAL_WINDOW_SIZE = 3;
const IDX_SESSION_STATE_LAST_PROC_STREAM_ID = 4;
const IDX_SESSION_STATE_REMOTE_WINDOW_SIZE = 5;
const IDX_SESSION_STATE_OUTBOUND_QUEUE_SIZE = 6;
const IDX_SESSION_STATE_HD_DEFLATE_DYNAMIC_TABLE_SIZE = 7;
const IDX_SESSION_STATE_HD_INFLATE_DYNAMIC_TABLE_SIZE = 8;
const IDX_STREAM_STATE = 0;
const IDX_STREAM_STATE_WEIGHT = 1;
const IDX_STREAM_STATE_SUM_DEPENDENCY_WEIGHT = 2;
const IDX_STREAM_STATE_LOCAL_CLOSE = 3;
const IDX_STREAM_STATE_REMOTE_CLOSE = 4;
const IDX_STREAM_STATE_LOCAL_WINDOW_SIZE = 5;

const IDX_OPTIONS_MAX_DEFLATE_DYNAMIC_TABLE_SIZE = 0;
const IDX_OPTIONS_MAX_RESERVED_REMOTE_STREAMS = 1;
const IDX_OPTIONS_MAX_SEND_HEADER_BLOCK_LENGTH = 2;
const IDX_OPTIONS_PEER_MAX_CONCURRENT_STREAMS = 3;
const IDX_OPTIONS_PADDING_STRATEGY = 4;
const IDX_OPTIONS_MAX_HEADER_LIST_PAIRS = 5;
const IDX_OPTIONS_MAX_OUTSTANDING_PINGS = 6;
const IDX_OPTIONS_MAX_OUTSTANDING_SETTINGS = 7;
const IDX_OPTIONS_MAX_SESSION_MEMORY = 8;
const IDX_OPTIONS_MAX_SETTINGS = 9;
const IDX_OPTIONS_FLAGS = 10;

const kMaxFrameSize = (2 ** 24) - 1;
const kMaxInt = (2 ** 32) - 1;
const kMaxStreams = (2 ** 32) - 1;

function updateOptionsBuffer(options) {
  let flags = 0;
  if (typeof options.maxDeflateDynamicTableSize === 'number') {
    flags |= (1 << IDX_OPTIONS_MAX_DEFLATE_DYNAMIC_TABLE_SIZE);
    optionsBuffer[IDX_OPTIONS_MAX_DEFLATE_DYNAMIC_TABLE_SIZE] =
      options.maxDeflateDynamicTableSize;
  }
  if (typeof options.maxReservedRemoteStreams === 'number') {
    flags |= (1 << IDX_OPTIONS_MAX_RESERVED_REMOTE_STREAMS);
    optionsBuffer[IDX_OPTIONS_MAX_RESERVED_REMOTE_STREAMS] =
      options.maxReservedRemoteStreams;
  }
  if (typeof options.maxSendHeaderBlockLength === 'number') {
    flags |= (1 << IDX_OPTIONS_MAX_SEND_HEADER_BLOCK_LENGTH);
    optionsBuffer[IDX_OPTIONS_MAX_SEND_HEADER_BLOCK_LENGTH] =
      options.maxSendHeaderBlockLength;
  }
  if (typeof options.peerMaxConcurrentStreams === 'number') {
    flags |= (1 << IDX_OPTIONS_PEER_MAX_CONCURRENT_STREAMS);
    optionsBuffer[IDX_OPTIONS_PEER_MAX_CONCURRENT_STREAMS] =
      options.peerMaxConcurrentStreams;
  }
  if (typeof options.paddingStrategy === 'number') {
    flags |= (1 << IDX_OPTIONS_PADDING_STRATEGY);
    optionsBuffer[IDX_OPTIONS_PADDING_STRATEGY] =
      options.paddingStrategy;
  }
  if (typeof options.maxHeaderListPairs === 'number') {
    flags |= (1 << IDX_OPTIONS_MAX_HEADER_LIST_PAIRS);
    optionsBuffer[IDX_OPTIONS_MAX_HEADER_LIST_PAIRS] =
      options.maxHeaderListPairs;
  }
  if (typeof options.maxOutstandingPings === 'number') {
    flags |= (1 << IDX_OPTIONS_MAX_OUTSTANDING_PINGS);
    optionsBuffer[IDX_OPTIONS_MAX_OUTSTANDING_PINGS] =
      options.maxOutstandingPings;
  }
  if (typeof options.maxOutstandingSettings === 'number') {
    flags |= (1 << IDX_OPTIONS_MAX_OUTSTANDING_SETTINGS);
    optionsBuffer[IDX_OPTIONS_MAX_OUTSTANDING_SETTINGS] =
      MathMax(1, options.maxOutstandingSettings);
  }
  if (typeof options.maxSessionMemory === 'number') {
    flags |= (1 << IDX_OPTIONS_MAX_SESSION_MEMORY);
    optionsBuffer[IDX_OPTIONS_MAX_SESSION_MEMORY] =
      MathMax(1, options.maxSessionMemory);
  }
  if (typeof options.maxSettings === 'number') {
    flags |= (1 << IDX_OPTIONS_MAX_SETTINGS);
    optionsBuffer[IDX_OPTIONS_MAX_SETTINGS] =
      MathMax(1, options.maxSettings);
  }
  optionsBuffer[IDX_OPTIONS_FLAGS] = flags;
}

function getDefaultSettings() {
  settingsBuffer[IDX_SETTINGS_FLAGS] = 0;
  binding.refreshDefaultSettings();
  const holder = ObjectCreate(null);

  const flags = settingsBuffer[IDX_SETTINGS_FLAGS];

  if ((flags & (1 << IDX_SETTINGS_HEADER_TABLE_SIZE)) ===
      (1 << IDX_SETTINGS_HEADER_TABLE_SIZE)) {
    holder.headerTableSize =
      settingsBuffer[IDX_SETTINGS_HEADER_TABLE_SIZE];
  }

  if ((flags & (1 << IDX_SETTINGS_ENABLE_PUSH)) ===
      (1 << IDX_SETTINGS_ENABLE_PUSH)) {
    holder.enablePush =
      settingsBuffer[IDX_SETTINGS_ENABLE_PUSH] === 1;
  }

  if ((flags & (1 << IDX_SETTINGS_INITIAL_WINDOW_SIZE)) ===
      (1 << IDX_SETTINGS_INITIAL_WINDOW_SIZE)) {
    holder.initialWindowSize =
      settingsBuffer[IDX_SETTINGS_INITIAL_WINDOW_SIZE];
  }

  if ((flags & (1 << IDX_SETTINGS_MAX_FRAME_SIZE)) ===
      (1 << IDX_SETTINGS_MAX_FRAME_SIZE)) {
    holder.maxFrameSize =
      settingsBuffer[IDX_SETTINGS_MAX_FRAME_SIZE];
  }

  if ((flags & (1 << IDX_SETTINGS_MAX_CONCURRENT_STREAMS)) ===
      (1 << IDX_SETTINGS_MAX_CONCURRENT_STREAMS)) {
    holder.maxConcurrentStreams =
      settingsBuffer[IDX_SETTINGS_MAX_CONCURRENT_STREAMS];
  }

  if ((flags & (1 << IDX_SETTINGS_MAX_HEADER_LIST_SIZE)) ===
      (1 << IDX_SETTINGS_MAX_HEADER_LIST_SIZE)) {
    holder.maxHeaderListSize =
      settingsBuffer[IDX_SETTINGS_MAX_HEADER_LIST_SIZE];
  }

  if ((flags & (1 << IDX_SETTINGS_ENABLE_CONNECT_PROTOCOL)) ===
      (1 << IDX_SETTINGS_ENABLE_CONNECT_PROTOCOL)) {
    holder.enableConnectProtocol =
      settingsBuffer[IDX_SETTINGS_ENABLE_CONNECT_PROTOCOL] === 1;
  }

  return holder;
}

// Remote is a boolean. true to fetch remote settings, false to fetch local.
// this is only called internally
function getSettings(session, remote) {
  if (remote)
    session.remoteSettings();
  else
    session.localSettings();

  return {
    headerTableSize: settingsBuffer[IDX_SETTINGS_HEADER_TABLE_SIZE],
    enablePush: !!settingsBuffer[IDX_SETTINGS_ENABLE_PUSH],
    initialWindowSize: settingsBuffer[IDX_SETTINGS_INITIAL_WINDOW_SIZE],
    maxFrameSize: settingsBuffer[IDX_SETTINGS_MAX_FRAME_SIZE],
    maxConcurrentStreams: settingsBuffer[IDX_SETTINGS_MAX_CONCURRENT_STREAMS],
    maxHeaderListSize: settingsBuffer[IDX_SETTINGS_MAX_HEADER_LIST_SIZE],
    enableConnectProtocol:
      !!settingsBuffer[IDX_SETTINGS_ENABLE_CONNECT_PROTOCOL]
  };
}

function updateSettingsBuffer(settings) {
  let flags = 0;
  if (typeof settings.headerTableSize === 'number') {
    flags |= (1 << IDX_SETTINGS_HEADER_TABLE_SIZE);
    settingsBuffer[IDX_SETTINGS_HEADER_TABLE_SIZE] =
      settings.headerTableSize;
  }
  if (typeof settings.maxConcurrentStreams === 'number') {
    flags |= (1 << IDX_SETTINGS_MAX_CONCURRENT_STREAMS);
    settingsBuffer[IDX_SETTINGS_MAX_CONCURRENT_STREAMS] =
      settings.maxConcurrentStreams;
  }
  if (typeof settings.initialWindowSize === 'number') {
    flags |= (1 << IDX_SETTINGS_INITIAL_WINDOW_SIZE);
    settingsBuffer[IDX_SETTINGS_INITIAL_WINDOW_SIZE] =
      settings.initialWindowSize;
  }
  if (typeof settings.maxFrameSize === 'number') {
    flags |= (1 << IDX_SETTINGS_MAX_FRAME_SIZE);
    settingsBuffer[IDX_SETTINGS_MAX_FRAME_SIZE] =
      settings.maxFrameSize;
  }
  if (typeof settings.maxHeaderListSize === 'number') {
    flags |= (1 << IDX_SETTINGS_MAX_HEADER_LIST_SIZE);
    settingsBuffer[IDX_SETTINGS_MAX_HEADER_LIST_SIZE] =
      settings.maxHeaderListSize;
  }
  if (typeof settings.enablePush === 'boolean') {
    flags |= (1 << IDX_SETTINGS_ENABLE_PUSH);
    settingsBuffer[IDX_SETTINGS_ENABLE_PUSH] = Number(settings.enablePush);
  }
  if (typeof settings.enableConnectProtocol === 'boolean') {
    flags |= (1 << IDX_SETTINGS_ENABLE_CONNECT_PROTOCOL);
    settingsBuffer[IDX_SETTINGS_ENABLE_CONNECT_PROTOCOL] =
      Number(settings.enableConnectProtocol);
  }

  settingsBuffer[IDX_SETTINGS_FLAGS] = flags;
}

function getSessionState(session) {
  session.refreshState();
  return {
    effectiveLocalWindowSize:
      sessionState[IDX_SESSION_STATE_EFFECTIVE_LOCAL_WINDOW_SIZE],
    effectiveRecvDataLength:
      sessionState[IDX_SESSION_STATE_EFFECTIVE_RECV_DATA_LENGTH],
    nextStreamID:
      sessionState[IDX_SESSION_STATE_NEXT_STREAM_ID],
    localWindowSize:
      sessionState[IDX_SESSION_STATE_LOCAL_WINDOW_SIZE],
    lastProcStreamID:
      sessionState[IDX_SESSION_STATE_LAST_PROC_STREAM_ID],
    remoteWindowSize:
      sessionState[IDX_SESSION_STATE_REMOTE_WINDOW_SIZE],
    outboundQueueSize:
      sessionState[IDX_SESSION_STATE_OUTBOUND_QUEUE_SIZE],
    deflateDynamicTableSize:
      sessionState[IDX_SESSION_STATE_HD_DEFLATE_DYNAMIC_TABLE_SIZE],
    inflateDynamicTableSize:
      sessionState[IDX_SESSION_STATE_HD_INFLATE_DYNAMIC_TABLE_SIZE]
  };
}

function getStreamState(stream) {
  stream.refreshState();
  return {
    state: streamState[IDX_STREAM_STATE],
    weight: streamState[IDX_STREAM_STATE_WEIGHT],
    sumDependencyWeight: streamState[IDX_STREAM_STATE_SUM_DEPENDENCY_WEIGHT],
    localClose: streamState[IDX_STREAM_STATE_LOCAL_CLOSE],
    remoteClose: streamState[IDX_STREAM_STATE_REMOTE_CLOSE],
    localWindowSize: streamState[IDX_STREAM_STATE_LOCAL_WINDOW_SIZE]
  };
}

function isIllegalConnectionSpecificHeader(name, value) {
  switch (name) {
    case HTTP2_HEADER_CONNECTION:
    case HTTP2_HEADER_UPGRADE:
    case HTTP2_HEADER_HOST:
    case HTTP2_HEADER_HTTP2_SETTINGS:
    case HTTP2_HEADER_KEEP_ALIVE:
    case HTTP2_HEADER_PROXY_CONNECTION:
    case HTTP2_HEADER_TRANSFER_ENCODING:
      return true;
    case HTTP2_HEADER_TE:
      return value !== 'trailers';
    default:
      return false;
  }
}

const assertValidPseudoHeader = hideStackFrames((key) => {
  if (!kValidPseudoHeaders.has(key)) {
    throw new ERR_HTTP2_INVALID_PSEUDOHEADER(key);
  }
});

const assertValidPseudoHeaderResponse = hideStackFrames((key) => {
  if (key !== ':status') {
    throw new ERR_HTTP2_INVALID_PSEUDOHEADER(key);
  }
});

const assertValidPseudoHeaderTrailer = hideStackFrames((key) => {
  throw new ERR_HTTP2_INVALID_PSEUDOHEADER(key);
});

const emptyArray = [];
const kNeverIndexFlag = String.fromCharCode(NGHTTP2_NV_FLAG_NO_INDEX);
const kNoHeaderFlags = String.fromCharCode(NGHTTP2_NV_FLAG_NONE);
function mapToHeaders(map,
                      assertValuePseudoHeader = assertValidPseudoHeader) {
  let ret = '';
  let count = 0;
  const keys = ObjectKeys(map);
  const singles = new Set();
  let i, j;
  let isArray;
  let key;
  let value;
  let isSingleValueHeader;
  let err;
  const neverIndex =
    (map[kSensitiveHeaders] || emptyArray).map(StringPrototypeToLowerCase);
  for (i = 0; i < keys.length; ++i) {
    key = keys[i];
    value = map[key];
    if (value === undefined || key === '')
      continue;
    key = key.toLowerCase();
    isSingleValueHeader = kSingleValueHeaders.has(key);
    isArray = ArrayIsArray(value);
    if (isArray) {
      switch (value.length) {
        case 0:
          continue;
        case 1:
          value = String(value[0]);
          isArray = false;
          break;
        default:
          if (isSingleValueHeader)
            throw new ERR_HTTP2_HEADER_SINGLE_VALUE(key);
      }
    } else {
      value = String(value);
    }
    if (isSingleValueHeader) {
      if (singles.has(key))
        throw new ERR_HTTP2_HEADER_SINGLE_VALUE(key);
      singles.add(key);
    }
    const flags = neverIndex.includes(key) ? kNeverIndexFlag : kNoHeaderFlags;
    if (key[0] === ':') {
      err = assertValuePseudoHeader(key);
      if (err !== undefined)
        throw err;
      ret = `${key}\0${value}\0${flags}${ret}`;
      count++;
      continue;
    }
    if (key.indexOf(' ') >= 0) {
      throw new ERR_INVALID_HTTP_TOKEN('Header name', key);
    }
    if (isIllegalConnectionSpecificHeader(key, value)) {
      throw new ERR_HTTP2_INVALID_CONNECTION_HEADERS(key);
    }
    if (isArray) {
      for (j = 0; j < value.length; ++j) {
        const val = String(value[j]);
        ret += `${key}\0${val}\0${flags}`;
      }
      count += value.length;
      continue;
    }
    ret += `${key}\0${value}\0${flags}`;
    count++;
  }

  return [ret, count];
}

class NghttpError extends Error {
  constructor(ret) {
    super(binding.nghttp2ErrorString(ret));
    this.code = 'ERR_HTTP2_ERROR';
    this.errno = ret;
    addCodeToName(this, super.name, 'ERR_HTTP2_ERROR');
  }

  toString() {
    return `${this.name} [${this.code}]: ${this.message}`;
  }
}

const assertIsObject = hideStackFrames((value, name, types) => {
  if (value !== undefined &&
      (value === null ||
       typeof value !== 'object' ||
       ArrayIsArray(value))) {
    throw new ERR_INVALID_ARG_TYPE(name, types || 'Object', value);
  }
});

const assertWithinRange = hideStackFrames(
  (name, value, min = 0, max = Infinity) => {
    if (value !== undefined &&
      (typeof value !== 'number' || value < min || value > max)) {
      throw new ERR_HTTP2_INVALID_SETTING_VALUE.RangeError(
        name, value, min, max);
    }
  }
);

function toHeaderObject(headers, sensitiveHeaders) {
  const obj = ObjectCreate(null);
  for (var n = 0; n < headers.length; n += 2) {
    const name = headers[n];
    let value = headers[n + 1];
    if (name === HTTP2_HEADER_STATUS)
      value |= 0;
    const existing = obj[name];
    if (existing === undefined) {
      obj[name] = name === HTTP2_HEADER_SET_COOKIE ? [value] : value;
    } else if (!kSingleValueHeaders.has(name)) {
      switch (name) {
        case HTTP2_HEADER_COOKIE:
          // https://tools.ietf.org/html/rfc7540#section-8.1.2.5
          // "...If there are multiple Cookie header fields after decompression,
          //  these MUST be concatenated into a single octet string using the
          //  two-octet delimiter of 0x3B, 0x20 (the ASCII string "; ") before
          //  being passed into a non-HTTP/2 context."
          obj[name] = `${existing}; ${value}`;
          break;
        case HTTP2_HEADER_SET_COOKIE:
          // https://tools.ietf.org/html/rfc7230#section-3.2.2
          // "Note: In practice, the "Set-Cookie" header field ([RFC6265]) often
          // appears multiple times in a response message and does not use the
          // list syntax, violating the above requirements on multiple header
          // fields with the same name.  Since it cannot be combined into a
          // single field-value, recipients ought to handle "Set-Cookie" as a
          // special case while processing header fields."
          existing.push(value);
          break;
        default:
          // https://tools.ietf.org/html/rfc7230#section-3.2.2
          // "A recipient MAY combine multiple header fields with the same field
          // name into one "field-name: field-value" pair, without changing the
          // semantics of the message, by appending each subsequent field value
          // to the combined field value in order, separated by a comma."
          obj[name] = `${existing}, ${value}`;
          break;
      }
    }
  }
  obj[kSensitiveHeaders] = sensitiveHeaders;
  return obj;
}

function isPayloadMeaningless(method) {
  return kNoPayloadMethods.has(method);
}

function sessionName(type) {
  switch (type) {
    case NGHTTP2_SESSION_CLIENT:
      return 'client';
    case NGHTTP2_SESSION_SERVER:
      return 'server';
    default:
      return '<invalid>';
  }
}

// Validates the values in a settings object. Specifically:
// 1. headerTableSize must be a number in the range 0 <= n <= kMaxInt
// 2. initialWindowSize must be a number in the range 0 <= n <= kMaxInt
// 3. maxFrameSize must be a number in the range 16384 <= n <= kMaxFrameSize
// 4. maxConcurrentStreams must be a number in the range 0 <= n <= kMaxStreams
// 5. maxHeaderListSize must be a number in the range 0 <= n <= kMaxInt
// 6. enablePush must be a boolean
// 7. enableConnectProtocol must be a boolean
// All settings are optional and may be left undefined
const validateSettings = hideStackFrames((settings) => {
  if (settings === undefined) return;
  assertWithinRange('headerTableSize',
                    settings.headerTableSize,
                    0, kMaxInt);
  assertWithinRange('initialWindowSize',
                    settings.initialWindowSize,
                    0, kMaxInt);
  assertWithinRange('maxFrameSize',
                    settings.maxFrameSize,
                    16384, kMaxFrameSize);
  assertWithinRange('maxConcurrentStreams',
                    settings.maxConcurrentStreams,
                    0, kMaxStreams);
  assertWithinRange('maxHeaderListSize',
                    settings.maxHeaderListSize,
                    0, kMaxInt);
  if (settings.enablePush !== undefined &&
      typeof settings.enablePush !== 'boolean') {
    throw new ERR_HTTP2_INVALID_SETTING_VALUE('enablePush',
                                              settings.enablePush);
  }
  if (settings.enableConnectProtocol !== undefined &&
      typeof settings.enableConnectProtocol !== 'boolean') {
    throw new ERR_HTTP2_INVALID_SETTING_VALUE('enableConnectProtocol',
                                              settings.enableConnectProtocol);
  }
});

// Validates that priority options are correct, specifically:
// 1. options.weight must be a number
// 2. options.parent must be a positive number
// 3. options.exclusive must be a boolean
// 4. if specified, options.silent must be a boolean
//
// Also sets the default priority options if they are not set.
const setAndValidatePriorityOptions = hideStackFrames((options) => {
  if (options.weight === undefined) {
    options.weight = NGHTTP2_DEFAULT_WEIGHT;
  } else if (typeof options.weight !== 'number') {
    throw new ERR_INVALID_OPT_VALUE('weight', options.weight);
  }

  if (options.parent === undefined) {
    options.parent = 0;
  } else if (typeof options.parent !== 'number' || options.parent < 0) {
    throw new ERR_INVALID_OPT_VALUE('parent', options.parent);
  }

  if (options.exclusive === undefined) {
    options.exclusive = false;
  } else if (typeof options.exclusive !== 'boolean') {
    throw new ERR_INVALID_OPT_VALUE('exclusive', options.exclusive);
  }

  if (options.silent === undefined) {
    options.silent = false;
  } else if (typeof options.silent !== 'boolean') {
    throw new ERR_INVALID_OPT_VALUE('silent', options.silent);
  }
});

function callTimeout(self, kSession) {
  // If the session is destroyed, this should never actually be invoked,
  // but just in case...
  if (self.destroyed)
    return;
  // This checks whether a write is currently in progress and also whether
  // that write is actually sending data across the write. The kHandle
  // stored `chunksSentSinceLastWrite` is only updated when a timeout event
  // happens, meaning that if a write is ongoing it should never equal the
  // newly fetched, updated value.
  if (self[kState].writeQueueSize > 0) {
    const handle = kSession ? self[kSession][kHandle] : self[kHandle];
    const chunksSentSinceLastWrite = handle !== undefined ?
      handle.chunksSentSinceLastWrite : null;
    if (chunksSentSinceLastWrite !== null &&
      chunksSentSinceLastWrite !== handle.updateChunksSent()) {
      self[kUpdateTimer]();
      return;
    }
  }

  self.emit('timeout');
}

module.exports = {
  assertIsObject,
  assertValidPseudoHeader,
  assertValidPseudoHeaderResponse,
  assertValidPseudoHeaderTrailer,
  assertWithinRange,
  callTimeout,
  getDefaultSettings,
  getSessionState,
  getSettings,
  getStreamState,
  isPayloadMeaningless,
  kSensitiveHeaders,
  kMaxFrameSize,
  kMaxInt,
  kMaxStreams,
  kSocket,
  kState,
  kProxySocket,
  kRequest,
  mapToHeaders,
  NghttpError,
  sessionName,
  setAndValidatePriorityOptions,
  toHeaderObject,
  updateOptionsBuffer,
  updateSettingsBuffer,
  validateSettings,
};
