'use strict';

const {
  JSONStringify,
  NumberIsInteger,
  ObjectAssign,
  ObjectDefineProperty,
  ObjectFreeze,
  ObjectPrototypeHasOwnProperty,
  PromisePrototypeThen,
  RangeError,
  StringPrototypeCharCodeAt,
  Symbol,
  TypeError,
} = primordials;

const {
  Headers,
  Request,
  Response,
  serverKit,
} = require('internal/deps/undici/undici');

const {
  kConstruct,
  HeadersList,
  fillHeaders,
  setHeadersList,
  setHeadersGuard,
  setRequestState,
  setRequestHeaders,
  setRequestSignal,
  getResponseState,
  setResponseState,
  setResponseHeaders,
} = serverKit;

const { URL } = require('internal/url');
const { ReadableStream } = require('internal/webstreams/readablestream');
const { AbortController } = require('internal/abort_controller');
const { Buffer } = require('buffer');
const { isUint8Array } = require('internal/util/types');

// Sentinel that routes the subclass constructors to the uninitialized
// (kConstruct) base path. Never exported, so user code cannot create
// uninitialized instances.
const kInternalConstruct = Symbol('kInternalConstruct');

function defineOwnValue(object, key, value) {
  ObjectDefineProperty(object, key, {
    __proto__: null,
    value,
    writable: true,
    enumerable: true,
    configurable: true,
  });
  return value;
}

function noop() {}

/**
 * Inner request state for requests produced by serve(). Shape-compatible with
 * undici's makeRequest() record: constant fields live on the prototype and
 * the expensive ones (url, urlList) materialize on first access, so creating
 * one costs four stores. Consumers that copy the record with an own-property
 * spread (cloneRequest) are handled by NodeRequest.prototype.clone().
 */
class ServerRequestState {
  constructor(method, fullUrl, headersList, body) {
    this.method = method;
    this.fullUrl = fullUrl;
    this.headersList = headersList;
    this.body = body;
  }

  get url() {
    return defineOwnValue(this, 'url', new URL(this.fullUrl));
  }

  set url(value) {
    defineOwnValue(this, 'url', value);
  }

  get urlList() {
    return defineOwnValue(this, 'urlList', [this.url]);
  }

  set urlList(value) {
    defineOwnValue(this, 'urlList', value);
  }
}

// Defaults mirror undici's makeRequest(), except mode: server requests were
// previously built through the public Request constructor, which sets 'cors'.
ObjectAssign(ServerRequestState.prototype, {
  localURLsOnly: false,
  unsafeRequest: false,
  client: null,
  reservedClient: null,
  replacesClientId: '',
  window: 'client',
  keepalive: false,
  serviceWorkers: 'all',
  initiator: '',
  destination: '',
  priority: null,
  origin: 'client',
  policyContainer: 'client',
  referrer: 'client',
  referrerPolicy: '',
  mode: 'cors',
  useCORSPreflightFlag: false,
  credentials: 'same-origin',
  useCredentials: false,
  cache: 'default',
  redirect: 'follow',
  integrity: '',
  cryptoGraphicsNonceMetadata: '',
  parserMetadata: '',
  reloadNavigation: false,
  historyNavigation: false,
  userActivation: false,
  taintedOrigin: false,
  redirectCount: 0,
  responseTainting: 'basic',
  preventNoCacheCacheControlHeaderModification: false,
  done: false,
  timingAllowFailed: false,
  traversableForUserPrompts: 'client',
});

let createServeRequest;
let getServeMetadata;
let abortServeRequest;

/**
 * A Request subclass whose serve() construction path skips the public
 * constructor entirely: no WebIDL conversion, no URL parse, no header
 * re-validation or copies, no AbortSignal until requested. Publicly
 * constructed instances (`new NodeRequest(url, init)`) behave exactly like
 * `new Request(url, init)`.
 */
class NodeRequest extends Request {
  #state = undefined;
  #meta = undefined;
  #signalController = undefined;
  #aborted = false;

  constructor(input, init = undefined) {
    if (input === kInternalConstruct) {
      super(kConstruct);
      return;
    }
    super(input, init);
  }

  get url() {
    return this.#state === undefined ? super.url : this.#state.fullUrl;
  }

  // The signal is created lazily: most handlers never observe it, and wiring
  // an EventTarget per request is measurable. It is written through to the
  // base class field so undici-internal reads observe it afterwards. The one
  // observable gap: `new Request(nodeRequest)` copies the private field
  // directly, so a request derived before .signal was ever accessed does not
  // follow client disconnects.
  get signal() {
    if (this.#state === undefined) {
      return super.signal;
    }
    let controller = this.#signalController;
    if (controller === undefined) {
      controller = new AbortController();
      this.#signalController = controller;
      setRequestSignal(this, controller.signal);
      if (this.#aborted) {
        controller.abort();
      }
    }
    return controller.signal;
  }

  get remoteAddress() {
    return this.#meta?.remoteAddress;
  }

  get remotePort() {
    return this.#meta?.remotePort;
  }

  get localAddress() {
    return this.#meta?.localAddress;
  }

  get localPort() {
    return this.#meta?.localPort;
  }

  get encrypted() {
    return this.#meta === undefined ? false : this.#meta.encrypted;
  }

  clone() {
    const state = this.#state;
    if (state !== undefined) {
      // cloneRequest() copies the inner state with an own-property spread.
      // Materialize the lazy fields and the one prototype default that
      // diverges from makeRequest()'s so they survive the copy, and the
      // signal so the clone's signal can follow this one.
      state.urlList; // eslint-disable-line no-unused-expressions
      defineOwnValue(state, 'mode', state.mode);
      this.signal; // eslint-disable-line no-unused-expressions
    }
    return super.clone();
  }

  static {
    createServeRequest = (method, fullUrl, headersList, body, meta) => {
      const request = new NodeRequest(kInternalConstruct);
      const state = new ServerRequestState(method, fullUrl, headersList, body);
      setRequestState(request, state);
      const headers = new Headers(kConstruct);
      setHeadersList(headers, headersList);
      setHeadersGuard(headers, 'immutable');
      setRequestHeaders(request, headers);
      request.#state = state;
      request.#meta = meta;
      return request;
    };

    getServeMetadata = (request) => {
      let meta;
      if (typeof request === 'object' && request !== null) {
        try {
          meta = request.#meta;
        } catch {
          // Not a NodeRequest.
        }
      }
      return meta;
    };

    abortServeRequest = (request) => {
      const controller = request.#signalController;
      if (controller !== undefined) {
        controller.abort();
      } else {
        request.#aborted = true;
      }
    };
  }
}

/**
 * Inner response state, shape-compatible with undici's makeResponse() record.
 */
class ServerResponseState {
  constructor(status, statusText, headersList, body) {
    this.status = status;
    this.statusText = statusText;
    this.headersList = headersList;
    this.body = body;
  }
}

const kEmptyUrlList = ObjectFreeze([]);

ObjectAssign(ServerResponseState.prototype, {
  aborted: false,
  rangeRequested: false,
  timingAllowPassed: false,
  requestIncludesCredentials: false,
  type: 'default',
  timingInfo: null,
  cacheState: '',
  urlList: kEmptyUrlList,
});

/**
 * Body record for string/Uint8Array response bodies: keeps the original
 * source and its byte length, and only materializes the ReadableStream if
 * something actually asks for it (res.body, mixins, clone). writeResponse()
 * writes the source directly and sets `used` instead.
 */
class LazyResponseBody {
  used = false;

  constructor(source, length) {
    this.source = source;
    this.length = length;
  }

  get stream() {
    return defineOwnValue(this, 'stream',
                          createSourceStream(this.source, this.used));
  }

  set stream(value) {
    defineOwnValue(this, 'stream', value);
  }
}

function hasMaterializedStream(body) {
  return ObjectPrototypeHasOwnProperty(body, 'stream');
}

function createSourceStream(source, used) {
  const stream = new ReadableStream({
    start(controller) {
      if (!used) {
        controller.enqueue(
          typeof source === 'string' ? Buffer.from(source, 'utf8') : source,
        );
      }
      controller.close();
    },
  });
  if (used) {
    // The body was already written to the socket: hand out a stream that is
    // both closed and disturbed, matching a fully consumed body.
    PromisePrototypeThen(stream.cancel(), noop, noop);
  }
  return stream;
}

// https://fetch.spec.whatwg.org/#reason-phrase
function isValidReasonPhrase(statusText) {
  for (let i = 0; i < statusText.length; ++i) {
    const c = StringPrototypeCharCodeAt(statusText, i);
    if (!(c === 0x09 || (c >= 0x20 && c <= 0x7e) || (c >= 0x80 && c <= 0xff))) {
      return false;
    }
  }
  return true;
}

function isFastResponseInit(init) {
  return init == null ||
    (typeof init === 'object' &&
     (init.status === undefined ||
      (typeof init.status === 'number' && NumberIsInteger(init.status))) &&
     (init.statusText === undefined || typeof init.statusText === 'string'));
}

/**
 * A Response subclass with a fast construction path for the bodies handlers
 * actually return: null, strings and Uint8Arrays, combined with plain-object
 * init. That path performs no WebIDL conversion and allocates no
 * ReadableStream. Anything else falls back to the standard Response
 * constructor, so behavior never diverges. Note that unlike Response, a
 * Uint8Array body is not copied; do not mutate it after passing it in.
 */
class NodeResponse extends Response {
  #state = undefined;

  constructor(body = null, init = undefined) {
    if (body === kInternalConstruct) {
      super(kConstruct);
      return;
    }
    if ((body === null || typeof body === 'string' || isUint8Array(body)) &&
        isFastResponseInit(init)) {
      super(kConstruct);
      NodeResponse.#init(this, body, init,
                         typeof body === 'string' ?
                           'text/plain;charset=UTF-8' : null);
      return;
    }
    super(body, init);
  }

  static #init(response, body, init, defaultContentType) {
    let status = 200;
    let statusText = '';
    let headersInit;
    if (init != null) {
      if (init.status !== undefined) {
        status = init.status;
        if (status < 200 || status > 599) {
          // Plain errors match what the Response constructor throws.
          // eslint-disable-next-line no-restricted-syntax
          throw new RangeError(
            'init["status"] must be in the range of 200 to 599, inclusive.');
        }
      }
      if (init.statusText !== undefined) {
        statusText = init.statusText;
        if (!isValidReasonPhrase(statusText)) {
          // eslint-disable-next-line no-restricted-syntax
          throw new TypeError('Invalid statusText');
        }
      }
      headersInit = init.headers;
    }
    const headersList = new HeadersList();
    const state = new ServerResponseState(status, statusText, headersList, null);
    setResponseState(response, state);
    const headers = new Headers(kConstruct);
    setHeadersList(headers, headersList);
    setHeadersGuard(headers, 'response');
    setResponseHeaders(response, headers);
    if (headersInit != null) {
      fillHeaders(headers, headersInit);
    }
    if (body !== null) {
      if (status === 204 || status === 205 || status === 304) {
        // eslint-disable-next-line no-restricted-syntax
        throw new TypeError('Response with null body status cannot have body');
      }
      state.body = new LazyResponseBody(
        body,
        typeof body === 'string' ? Buffer.byteLength(body) : body.byteLength,
      );
      if (defaultContentType !== null &&
          !headersList.contains('content-type', true)) {
        headersList.append('content-type', defaultContentType, true);
      }
    }
    response.#state = state;
  }

  get bodyUsed() {
    const state = this.#state;
    if (state !== undefined && state.body !== null &&
        !hasMaterializedStream(state.body)) {
      return state.body.used;
    }
    return super.bodyUsed;
  }

  static json(data, init = undefined) {
    if (!isFastResponseInit(init)) {
      return Response.json(data, init);
    }
    const text = JSONStringify(data);
    if (typeof text !== 'string') {
      // eslint-disable-next-line no-restricted-syntax
      throw new TypeError('The data is not JSON serializable');
    }
    const response = new NodeResponse(kInternalConstruct);
    NodeResponse.#init(response, text, init, 'application/json');
    return response;
  }
}

function isLazyResponseBody(body) {
  return body instanceof LazyResponseBody;
}

module.exports = {
  NodeRequest,
  NodeResponse,
  createServeRequest,
  getServeMetadata,
  abortServeRequest,
  getResponseState,
  HeadersList,
  isLazyResponseBody,
  hasMaterializedStream,
};
