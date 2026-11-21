'use strict';

// The ServerRequest protocol of the Fetch Server API.
// https://fetch-server.proposal.wintertc.org/#serverrequest

const {
  ObjectDefineProperties,
  PromisePrototypeThen,
  PromiseReject,
  PromiseResolve,
  SymbolAsyncIterator,
  SymbolToStringTag,
} = primordials;

const {
  codes: {
    ERR_INVALID_STATE,
    ERR_INVALID_THIS,
  },
} = require('internal/errors');

const {
  customInspectSymbol: kInspect,
  kEnumerableProperty,
} = require('internal/util');

const { inspect } = require('internal/util/inspect');

const {
  createAsyncIterator,
  createAsyncIteratorPrototype,
} = require('internal/http/fetch_server/body');

const {
  createProtocolError,
  invalidState,
  kContentTooLarge,
} = require('internal/http/fetch_server/errors');

const {
  createServerHeaders,
} = require('internal/http/fetch_server/headers');

const kAsyncIteratorPrototype = createAsyncIteratorPrototype('ServerRequest');

const kConstruct = {};

let getRequestExchange;
let isServerRequest;

class ServerRequest {
  #exchange;
  #method;
  #url;
  #headers;
  #bodyReader = null;
  #bodyEnded = false;
  #bytesRead = 0;
  #started = false;

  static {
    getRequestExchange = (request) => request.#exchange;
    isServerRequest = (value) => {
      return value != null && typeof value === 'object' && #exchange in value;
    };
  }

  constructor(key, exchange, method, url, headerList) {
    if (key !== kConstruct) {
      throw new ERR_INVALID_THIS('ServerRequest cannot be constructed');
    }
    this.#exchange = exchange;
    this.#method = method;
    this.#url = url;
    this.#headers = createServerHeaders(headerList, 'immutable');
  }

  get method() {
    return checkThis(this).#method;
  }

  get url() {
    return checkThis(this).#url;
  }

  get headers() {
    return checkThis(this).#headers;
  }

  get signal() {
    return checkThis(this).#exchange.signal;
  }

  get trailers() {
    return checkThis(this).#exchange.trailers;
  }

  [SymbolAsyncIterator]() {
    checkThis(this);
    let iterator;
    const next = () => this.#next(iterator);
    const returnSteps = () => {
      if (iterator === this.#bodyReader && !this.#bodyEnded) {
        this.#bodyEnded = true;
        this.#exchange.refuseBody();
      }
    };
    iterator = createAsyncIterator(kAsyncIteratorPrototype, next, returnSteps);
    this.#bodyReader ??= iterator;
    return iterator;
  }

  #next(iterator) {
    const exchange = this.#exchange;
    if (iterator !== this.#bodyReader) {
      return PromiseReject(new ERR_INVALID_STATE.TypeError(
        'The ServerRequest body has already been read'));
    }
    if (exchange.kind === 'connect') {
      return PromiseReject(invalidState(
        'The request of a CONNECT exchange has no content'));
    }
    if (exchange.state === 'aborted') {
      return PromiseReject(exchange.abortReason);
    }
    if (this.#bodyEnded) return PromiseResolve(null);
    if (!this.#started) {
      this.#started = true;
      // Beginning to read the body resolves a pending 100 obligation
      // (Section 7.4.1).
      if (exchange.pending100 && exchange.autoContinue &&
          exchange.responseCapable) {
        exchange.pending100 = false;
        exchange.state = 'informational';
        exchange.backend.sendContinue([]);
      }
    }
    if (exchange.bodyRefused) {
      // The body is no longer being received, e.g. because the exchange
      // became complete or denied first.
      this.#bodyEnded = true;
      return PromiseResolve(null);
    }
    const read = PromisePrototypeThen(
      exchange.backend.readBody(exchange),
      (batch) => {
        if (batch === null) {
          this.#bodyEnded = true;
          return null;
        }
        const limit = exchange.maxRequestBodySize;
        if (limit > 0) {
          for (let i = 0; i < batch.length; i++) {
            this.#bytesRead += batch[i].byteLength;
          }
          if (this.#bytesRead > limit) {
            this.#bodyEnded = true;
            const err = createProtocolError(
              kContentTooLarge, 'The request body exceeds the configured limit');
            exchange.failTrailers(err);
            exchange.refuseBody();
            throw err;
          }
        }
        return batch;
      },
      (err) => {
        this.#bodyEnded = true;
        throw exchange.state === 'aborted' ? exchange.abortReason : err;
      });
    return exchange.raceAbort(read);
  }

  [kInspect](depth, options) {
    if (depth < 0) return 'ServerRequest {}';
    const opts = {
      __proto__: null,
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };
    return `ServerRequest ${inspect({
      method: this.#method,
      url: this.#url,
      headers: this.#headers,
    }, opts)}`;
  }
}

function checkThis(self) {
  if (!isServerRequest(self)) {
    throw new ERR_INVALID_THIS('ServerRequest');
  }
  return self;
}

ObjectDefineProperties(ServerRequest.prototype, {
  method: kEnumerableProperty,
  url: kEnumerableProperty,
  headers: kEnumerableProperty,
  signal: kEnumerableProperty,
  trailers: kEnumerableProperty,
  [SymbolToStringTag]: {
    __proto__: null,
    configurable: true,
    value: 'ServerRequest',
  },
});

function createServerRequest(exchange, method, url, headerList) {
  return new ServerRequest(kConstruct, exchange, method, url, headerList);
}

module.exports = {
  createServerRequest,
  getRequestExchange,
  isServerRequest,
};
