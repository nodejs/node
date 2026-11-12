'use strict';

// The ServerResponse interface of the Fetch Server API.
// https://fetch-server.proposal.wintertc.org/#serverresponse

const {
  ArrayPrototypeIncludes,
  ArrayPrototypePush,
  JSONStringify,
  ObjectDefineProperties,
  PromisePrototypeThen,
  PromiseReject,
  PromiseResolve,
  PromiseWithResolvers,
  RegExpPrototypeExec,
  SafeSet,
  StringPrototypeSplit,
  StringPrototypeToLowerCase,
  StringPrototypeTrim,
  SymbolAsyncIterator,
  SymbolIterator,
  SymbolToStringTag,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_STATE,
    ERR_INVALID_THIS,
    ERR_OUT_OF_RANGE,
  },
} = require('internal/errors');

const {
  customInspectSymbol: kInspect,
  kEnumerableProperty,
} = require('internal/util');

const { inspect } = require('internal/util/inspect');

const {
  isAnyArrayBuffer,
  isArrayBufferView,
} = require('internal/util/types');

const {
  converters,
} = require('internal/webidl');

const {
  AsyncSequenceSource,
  BytesSource,
  LazySource,
  ReadableStreamSource,
  copyBufferSource,
  createAsyncIterator,
  createAsyncIteratorPrototype,
  markHandled,
  splitSource,
} = require('internal/http/fetch_server/body');

const {
  convertHeadersInit,
  copyList,
  createServerHeaders,
  getHeaderList,
  listContains,
  listDelete,
  listGet,
  toByteString,
} = require('internal/http/fetch_server/headers');

let encoder;
function encodeUtf8(string) {
  encoder ??= new (require('internal/encoding').TextEncoder)();
  return encoder.encode(string);
}

let undici;
function lazyUndici() {
  undici ??= require('internal/deps/undici/undici');
  return undici;
}

let readableStreamHelpers;
function lazyReadableStream() {
  readableStreamHelpers ??= require('internal/webstreams/readablestream');
  return readableStreamHelpers;
}

let isBlobFn;
function isBlob(value) {
  isBlobFn ??= require('internal/blob').isBlob;
  return isBlobFn(value);
}

let URLSearchParamsCtor;
function isURLSearchParams(value) {
  URLSearchParamsCtor ??= require('internal/url').URLSearchParams;
  return value instanceof URLSearchParamsCtor;
}

function isFormData(value) {
  return value instanceof lazyUndici().FormData;
}

function isReadableStream(value) {
  return lazyReadableStream().isReadableStream(value);
}

const kNullBodyStatuses = [101, 103, 204, 205, 304];
const kDecodedEncodings = new SafeSet(['gzip', 'x-gzip', 'deflate', 'br', 'zstd']);
const kRedirectStatuses = [301, 302, 303, 307, 308];
// reason-phrase = 1*( HTAB / SP / VCHAR / obs-text )
const kReasonPhraseRegExp = /^[\t\x20-\x7e\x80-\xff]+$/;
// The characters of a URI-reference (RFC 3986 Section 4.1).
const kUriReferenceRegExp =
  /^(?:[A-Za-z0-9\-._~:/?#[\]@!$&'()*+,;=]|%[0-9A-Fa-f]{2})*$/;

// =============================================================================
// Trailer state (Section 9.4)
// =============================================================================

class TrailerState {
  list = [];
  state = 'pending';
  error = undefined;
  waiters = [];

  settle() {
    const waiters = this.waiters;
    this.waiters = [];
    for (let i = 0; i < waiters.length; i++) waiters[i]();
  }

  // Returns a promise that settles once the state leaves "pending".
  wait() {
    if (this.state !== 'pending') return PromiseResolve();
    const { promise, resolve } = PromiseWithResolvers();
    ArrayPrototypePush(this.waiters, resolve);
    return promise;
  }

  complete(list) {
    if (this.state !== 'pending') return;
    this.list = list;
    this.state = 'complete';
    this.settle();
  }

  fail(error) {
    if (this.state !== 'pending') return;
    this.error = error;
    this.state = 'errored';
    this.settle();
  }
}

function completeTrailerState() {
  const state = new TrailerState();
  state.state = 'complete';
  return state;
}

function getDeclaredTrailers(headerList) {
  const value = listGet(headerList, 'trailer');
  if (value === null) return [];
  const names = StringPrototypeSplit(value, ',');
  const declared = [];
  for (let i = 0; i < names.length; i++) {
    const name = StringPrototypeToLowerCase(StringPrototypeTrim(names[i]));
    if (name !== '') ArrayPrototypePush(declared, name);
  }
  return declared;
}

/**
 * Processes a trailers init (Section 9.4).
 * @param {TrailerState} state
 * @param {Array} headerList The response's header list.
 * @param {Promise<any>} trailersPromise
 */
function processTrailersInit(state, headerList, trailersPromise) {
  PromisePrototypeThen(trailersPromise, (init) => {
    let trailers;
    try {
      trailers = convertHeadersInit(init, 'trailers');
    } catch (err) {
      state.fail(err);
      return;
    }
    const declared = getDeclaredTrailers(headerList);
    const filtered = [];
    for (let i = 0; i < trailers.length; i++) {
      if (ArrayPrototypeIncludes(declared,
                                 StringPrototypeToLowerCase(trailers[i][0]))) {
        ArrayPrototypePush(filtered, trailers[i]);
      }
    }
    state.complete(filtered);
  }, (reason) => {
    state.fail(reason);
  });
}

// =============================================================================
// Extracting a body (Section 9.3)
// =============================================================================

function isAsyncSequence(value) {
  if (value === null ||
      (typeof value !== 'object' && typeof value !== 'function')) {
    return false;
  }
  return typeof value[SymbolAsyncIterator] === 'function' ||
         typeof value[SymbolIterator] === 'function';
}

/**
 * Extracts a response body from a ServerBodyInit (Section 9.3).
 * @param {any} object
 * @returns {{ source: object, type: string|null }}
 */
function extractBody(object) {
  if (isReadableStream(object)) {
    const { isReadableStreamLocked } = lazyReadableStream();
    if (isReadableStreamLocked(object) ||
        require('internal/streams/utils').isDisturbed(object)) {
      throw new ERR_INVALID_STATE.TypeError(
        'The ReadableStream is locked or disturbed');
    }
    return { source: new ReadableStreamSource(object), type: null };
  }
  if (isBlob(object)) {
    const type = object.type;
    return {
      source: new LazySource(async () => {
        const buffer = await object.arrayBuffer();
        return new BytesSource(copyBufferSource(buffer));
      }),
      type: type === '' ? null : type,
    };
  }
  if (isAnyArrayBuffer(object) || isArrayBufferView(object)) {
    return { source: new BytesSource(copyBufferSource(object)), type: null };
  }
  if (isFormData(object)) {
    const response = new (lazyUndici().Response)(object);
    return {
      source: new ReadableStreamSource(response.body),
      type: response.headers.get('content-type'),
    };
  }
  if (isURLSearchParams(object)) {
    return {
      source: new BytesSource(encodeUtf8(`${object}`)),
      type: 'application/x-www-form-urlencoded;charset=UTF-8',
    };
  }
  if (typeof object !== 'string' && isAsyncSequence(object)) {
    return { source: new AsyncSequenceSource(object), type: null };
  }
  return {
    source: new BytesSource(encodeUtf8(converters.USVString(object))),
    type: 'text/plain;charset=UTF-8',
  };
}

// =============================================================================
// ServerResponse
// =============================================================================

const kAsyncIteratorPrototype = createAsyncIteratorPrototype('ServerResponse');

let getResponseState;
let isServerResponse;

const kCreate = {};

function convertInit(init) {
  if (init === undefined || init === null) {
    return { __proto__: null, status: 200, statusText: '' };
  }
  if (typeof init !== 'object' && typeof init !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('init', 'Object', init);
  }
  // Web IDL dictionaries are converted in lexicographic order of members.
  const result = { __proto__: null };
  const headers = init.headers;
  if (headers !== undefined) result.headers = convertHeadersInit(headers);
  const status = init.status;
  result.status = status === undefined ? 200 :
    converters['unsigned short'](status, {
      __proto__: null,
      enforceRange: true,
      prefix: 'Failed to construct \'ServerResponse\'',
      context: 'status',
    });
  const statusText = init.statusText;
  result.statusText = statusText === undefined ? '' :
    toByteString(statusText, 'statusText');
  const trailers = init.trailers;
  if (trailers !== undefined) result.trailers = PromiseResolve(trailers);
  return result;
}

/**
 * Initializes a server response (Section 9.1).
 * @param {object} state
 * @param {object} init A converted ServerResponseInit.
 * @param {{ source: object, type: string|null }|null} bodyWithType
 */
function initializeResponse(state, init, bodyWithType) {
  if (init.status < 200 || init.status > 599) {
    throw new ERR_OUT_OF_RANGE('init.status', '>= 200 and <= 599', init.status);
  }
  if (init.statusText !== '' &&
      RegExpPrototypeExec(kReasonPhraseRegExp, init.statusText) === null) {
    throw new ERR_INVALID_ARG_VALUE('init.statusText', init.statusText,
                                    'must match the reason-phrase production');
  }
  state.status = init.status;
  state.statusText = init.statusText;
  const list = getHeaderList(state.headers);
  if (init.headers !== undefined) {
    for (let i = 0; i < init.headers.length; i++) {
      ArrayPrototypePush(list, init.headers[i]);
    }
  }
  if (bodyWithType !== null) {
    if (ArrayPrototypeIncludes(kNullBodyStatuses, state.status)) {
      throw new ERR_INVALID_ARG_VALUE('init.status', state.status,
                                      'cannot have a body');
    }
    state.body = bodyWithType.source;
    if (bodyWithType.type !== null && !listContains(list, 'content-type')) {
      ArrayPrototypePush(list, ['Content-Type', bodyWithType.type]);
    }
  }
  if (init.trailers !== undefined) {
    processTrailersInit(state.trailerState, list, init.trailers);
  } else {
    state.trailerState = completeTrailerState();
  }
}

class ServerResponse {
  #state;

  static {
    getResponseState = (response) => response.#state;
    isServerResponse = (value) => {
      return value != null && typeof value === 'object' && #state in value;
    };
  }

  constructor(body = null, init = undefined) {
    if (body === kCreate) {
      // Create a server response (Section 9).
      this.#state = createState();
      return;
    }
    // Web IDL converts the arguments before running the constructor steps.
    const convertedInit = convertInit(init);
    this.#state = createState();
    let bodyWithType = null;
    if (body !== null && body !== undefined) {
      bodyWithType = extractBody(body);
    }
    initializeResponse(this.#state, convertedInit, bodyWithType);
  }

  static error() {
    const response = new ServerResponse(kCreate);
    const state = response.#state;
    state.type = 'error';
    state.status = 0;
    state.trailerState = completeTrailerState();
    return response;
  }

  static redirect(url, status = 302) {
    url = converters.USVString(url, {
      __proto__: null,
      prefix: 'Failed to execute \'redirect\' on \'ServerResponse\'',
      context: 'url',
    });
    status = converters['unsigned short'](status, {
      __proto__: null,
      enforceRange: true,
      prefix: 'Failed to execute \'redirect\' on \'ServerResponse\'',
      context: 'status',
    });
    if (RegExpPrototypeExec(kUriReferenceRegExp, url) === null) {
      throw new ERR_INVALID_ARG_VALUE('url', url,
                                      'must be a valid URI-reference');
    }
    if (!ArrayPrototypeIncludes(kRedirectStatuses, status)) {
      throw new ERR_OUT_OF_RANGE('status', 'a redirect status', status);
    }
    const response = new ServerResponse(kCreate);
    const state = response.#state;
    state.status = status;
    ArrayPrototypePush(getHeaderList(state.headers), ['Location', url]);
    state.trailerState = completeTrailerState();
    return response;
  }

  static json(data, init = undefined) {
    const convertedInit = convertInit(init);
    const json = JSONStringify(data);
    if (json === undefined) {
      throw new ERR_INVALID_ARG_VALUE('data', data,
                                      'cannot be serialized to JSON');
    }
    const response = new ServerResponse(kCreate);
    initializeResponse(response.#state, convertedInit, {
      source: new BytesSource(encodeUtf8(json)),
      type: 'application/json',
    });
    return response;
  }

  static from(response) {
    const { Response } = lazyUndici();
    if (!(response instanceof Response)) {
      throw new ERR_INVALID_ARG_TYPE('response', 'Response', response);
    }
    const type = response.type;
    if (type === 'opaque' || type === 'opaqueredirect') {
      throw new ERR_INVALID_ARG_VALUE('response', response,
                                      'must not be an opaque response');
    }
    if (response.bodyUsed || response.body?.locked) {
      throw new ERR_INVALID_STATE.TypeError('Response body is unusable');
    }
    const result = new ServerResponse(kCreate);
    const state = result.#state;
    if (type === 'error') {
      state.type = 'error';
      state.status = 0;
      state.trailerState = completeTrailerState();
      return result;
    }
    state.status = response.status;
    state.statusText = response.statusText;
    const list = getHeaderList(state.headers);
    for (const { 0: name, 1: value } of response.headers) {
      ArrayPrototypePush(list, [name, value]);
    }
    // Bodies obtained from fetch() have been decoded according to their
    // Content-Encoding.
    if (response.url !== '' && listContains(list, 'content-encoding')) {
      const encoding = StringPrototypeToLowerCase(
        listGet(list, 'content-encoding'));
      if (kDecodedEncodings.has(encoding)) {
        listDelete(list, 'content-encoding');
        listDelete(list, 'content-length');
      }
    }
    if (response.body !== null) {
      state.body = new ReadableStreamSource(response.body);
    }
    const trailers = response.trailers;
    if (trailers !== undefined && trailers !== null) {
      processTrailersInit(state.trailerState, list, PromiseResolve(trailers));
    } else {
      state.trailerState = completeTrailerState();
    }
    return result;
  }

  get type() {
    return checkThis(this).type;
  }

  get url() {
    checkThis(this);
    return '';
  }

  get redirected() {
    checkThis(this);
    return false;
  }

  get status() {
    return checkThis(this).status;
  }

  get ok() {
    const { status } = checkThis(this);
    return status >= 200 && status <= 299;
  }

  get statusText() {
    return checkThis(this).statusText;
  }

  get headers() {
    return checkThis(this).headers;
  }

  get trailers() {
    const state = checkThis(this);
    if (state.trailersPromise === null) {
      const { promise, resolve, reject } = PromiseWithResolvers();
      state.trailersPromise = promise;
      markHandled(promise);
      const trailerState = state.trailerState;
      PromisePrototypeThen(trailerState.wait(), () => {
        if (trailerState.state === 'complete') {
          resolve(createServerHeaders(copyList(trailerState.list), 'immutable'));
        } else {
          reject(trailerState.error);
        }
      });
    }
    return state.trailersPromise;
  }

  clone() {
    const state = checkThis(this);
    if (isUnusable(state)) {
      throw new ERR_INVALID_STATE.TypeError('ServerResponse body is unusable');
    }
    const clone = new ServerResponse(kCreate);
    const cloneState = clone.#state;
    cloneState.type = state.type;
    cloneState.status = state.status;
    cloneState.statusText = state.statusText;
    const cloneList = getHeaderList(cloneState.headers);
    const list = getHeaderList(state.headers);
    for (let i = 0; i < list.length; i++) {
      ArrayPrototypePush(cloneList, [list[i][0], list[i][1]]);
    }
    cloneState.trailerState = state.trailerState;
    if (state.body !== null) {
      const { 0: first, 1: second } = splitSource(state.body);
      state.body = first;
      cloneState.body = second;
    }
    return clone;
  }

  values() {
    return createBodyIterator(checkThis(this));
  }

  [kInspect](depth, options) {
    if (depth < 0) return 'ServerResponse {}';
    const state = this.#state;
    const opts = {
      __proto__: null,
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };
    return `ServerResponse ${inspect({
      type: state.type,
      status: state.status,
      statusText: state.statusText,
      headers: state.headers,
      bodyUsed: state.bodyReader !== null,
    }, opts)}`;
  }
}

function createState() {
  return {
    __proto__: null,
    type: 'default',
    status: 200,
    statusText: '',
    headers: createServerHeaders(),
    body: null,
    bodyReader: null,
    bodyEnded: false,
    trailerState: new TrailerState(),
    trailersPromise: null,
  };
}

function checkThis(self) {
  if (!isServerResponse(self)) {
    throw new ERR_INVALID_THIS('ServerResponse');
  }
  return getResponseState(self);
}

function isUnusable(state) {
  return state.body !== null && state.bodyReader !== null;
}

/**
 * Acquires an iterator over the body of a ServerResponse (Section 9.3). The first
 * iterator obtained becomes the body reader; any later one rejects on next().
 * @param {object} state
 * @returns {object}
 */
function createBodyIterator(state) {
  let iterator;
  const next = () => {
    if (iterator !== state.bodyReader) {
      return PromiseReject(new ERR_INVALID_STATE.TypeError(
        'The ServerResponse body has already been read'));
    }
    if (state.body === null) return PromiseResolve(null);
    return PromisePrototypeThen(state.body.read(), (batch) => {
      if (batch === null) state.bodyEnded = true;
      return batch;
    }, (err) => {
      state.bodyEnded = true;
      throw err;
    });
  };
  const returnSteps = (reason) => {
    if (iterator === state.bodyReader && state.body !== null &&
        !state.bodyEnded) {
      state.bodyEnded = true;
      state.body.close(reason);
    }
  };
  iterator = createAsyncIterator(kAsyncIteratorPrototype, next, returnSteps);
  state.bodyReader ??= iterator;
  return iterator;
}

/**
 * Internal reader used by the implementation to send a response body.
 * @param {ServerResponse} response
 * @returns {object} An object with read() and close(reason) methods.
 */
function acquireBodyReader(response) {
  const state = getResponseState(response);
  const reader = {};
  state.bodyReader = reader;
  return {
    __proto__: null,
    read() {
      if (state.body === null || state.bodyEnded) return PromiseResolve(null);
      return PromisePrototypeThen(state.body.read(), (batch) => {
        if (batch === null) state.bodyEnded = true;
        return batch;
      }, (err) => {
        state.bodyEnded = true;
        throw err;
      });
    },
    close(reason) {
      if (state.body === null || state.bodyEnded) return;
      state.bodyEnded = true;
      state.body.close(reason);
    },
  };
}

function isResponseUnusable(response) {
  return isUnusable(getResponseState(response));
}

ObjectDefineProperties(ServerResponse.prototype, {
  type: kEnumerableProperty,
  url: kEnumerableProperty,
  redirected: kEnumerableProperty,
  status: kEnumerableProperty,
  ok: kEnumerableProperty,
  statusText: kEnumerableProperty,
  headers: kEnumerableProperty,
  trailers: kEnumerableProperty,
  clone: kEnumerableProperty,
  values: kEnumerableProperty,
  [SymbolAsyncIterator]: {
    __proto__: null,
    configurable: true,
    writable: true,
    value: ServerResponse.prototype.values,
  },
  [SymbolToStringTag]: {
    __proto__: null,
    configurable: true,
    value: 'ServerResponse',
  },
});

ObjectDefineProperties(ServerResponse, {
  error: kEnumerableProperty,
  redirect: kEnumerableProperty,
  json: kEnumerableProperty,
  from: kEnumerableProperty,
});

module.exports = {
  ServerResponse,
  acquireBodyReader,
  getDeclaredTrailers,
  getResponseState,
  isResponseUnusable,
  isServerResponse,
};
