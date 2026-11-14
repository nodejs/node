'use strict';

// Handler invocation (Section 16) and response sending (Section 9.6) for the Fetch
// Server API.
// https://fetch-server.proposal.wintertc.org/#handler-model

const {
  FunctionPrototypeCall,
  TypedArrayPrototypeGetByteLength,
} = primordials;

const {
  codes: {
    ERR_INVALID_RETURN_VALUE,
  },
} = require('internal/errors');

const {
  createProtocolError,
  getErrorCode,
  kCommittedSendable,
  kConnectError,
  kContentTooLarge,
  kInternalError,
  kRequestCancelled,
} = require('internal/http/fetch_server/errors');

const {
  filterTrailers,
  getHeaderList,
  setHeadersImmutable,
} = require('internal/http/fetch_server/headers');

const {
  ServerResponse,
  acquireBodyReader,
  getDeclaredTrailers,
  getResponseState,
  isResponseUnusable,
  isServerResponse,
} = require('internal/http/fetch_server/response');

const {
  createContext,
} = require('internal/http/fetch_server/context');

const {
  report,
} = require('internal/http/fetch_server/exchange');

let undiciResponse;
function isFetchResponse(value) {
  undiciResponse ??= require('internal/deps/undici/undici').Response;
  return value instanceof undiciResponse;
}

function invalidReturn(value) {
  return new ERR_INVALID_RETURN_VALUE('a ServerResponse', 'handler', value);
}

/**
 * Sends a ServerResponse for an exchange (Section 9.6).
 * @param {import('./exchange').Exchange} exchange
 * @param {ServerResponse} response
 * @returns {Promise<void>}
 */
async function sendResponse(exchange, response) {
  const backend = exchange.backend;
  const state = getResponseState(response);
  exchange.commit();
  // A pending 100 obligation is omitted (Section 7.4.1).
  exchange.pending100 = false;
  setHeadersImmutable(state.headers);

  if (state.type === 'error') {
    backend.reset(kInternalError);
    exchange.complete();
    return;
  }

  const headerList = getHeaderList(state.headers);
  const isHead = backend.method === 'HEAD';
  const reader = acquireBodyReader(response);
  const wantTrailers = !isHead &&
    getDeclaredTrailers(headerList).length > 0 &&
    backend.canSendTrailers(headerList);
  // A body held as bytes, such as one from a string or a Uint8Array, is sent
  // with its length in a single write.
  const bytes = isHead || wantTrailers ? undefined : reader.takeBytes();
  const hasBody = state.body !== null && !isHead &&
    (bytes === undefined || TypedArrayPrototypeGetByteLength(bytes) > 0);
  const contentLength = bytes === undefined ? undefined :
    TypedArrayPrototypeGetByteLength(bytes);

  try {
    backend.respond(state.status, state.statusText, headerList,
                    { hasBody, wantTrailers, contentLength });
  } catch (err) {
    reader.close(err);
    report(err);
    backend.reset(kInternalError);
    exchange.complete();
    return;
  }

  if (isHead) reader.close();

  try {
    if (bytes !== undefined) {
      await exchange.raceAbort(backend.end(null, hasBody ? bytes : null));
      exchange.complete();
      return;
    }
    if (hasBody) {
      for (;;) {
        const batch = await exchange.raceAbort(reader.read());
        if (batch === null) break;
        await exchange.raceAbort(backend.write(batch));
      }
    }
    if (wantTrailers) {
      const trailerState = state.trailerState;
      await exchange.raceAbort(trailerState.wait());
      if (trailerState.state === 'errored') {
        backend.reset(getErrorCode(trailerState.error, kCommittedSendable) ??
                      kInternalError);
        exchange.complete();
        return;
      }
      const trailers = filterTrailers(trailerState.list);
      await exchange.raceAbort(backend.end(trailers));
    } else {
      await exchange.raceAbort(backend.end(null));
    }
  } catch (err) {
    if (exchange.state === 'aborted') {
      reader.close(exchange.abortReason);
      return;
    }
    reader.close(err);
    backend.reset(getErrorCode(err, kCommittedSendable) ?? kInternalError);
    exchange.complete();
    return;
  }
  exchange.complete();
}

function fallbackStatus(exchange, error) {
  if (error?.code === kContentTooLarge) return 413;
  return exchange.kind === 'connect' ? 502 : 500;
}

function sendFallback(exchange, status) {
  return sendResponse(exchange, new ServerResponse(null, { status }));
}

/**
 * Checks a value returned by a handler; returns a ServerResponse to send, or
 * throws a TypeError for an invalid return.
 * @param {import('./exchange').Exchange} exchange
 * @param {any} value
 * @returns {ServerResponse}
 */
function toResponse(exchange, value) {
  if (isFetchResponse(value)) {
    value = ServerResponse.from(value);
  }
  if (!isServerResponse(value) || isResponseUnusable(value)) {
    throw invalidReturn(value);
  }
  if (exchange.kind === 'connect') {
    const { status } = getResponseState(value);
    if (status >= 200 && status <= 299) {
      throw new ERR_INVALID_RETURN_VALUE(
        'a non-2xx ServerResponse', 'connect', value);
    }
  }
  return value;
}

async function invoke(fn, thisArg, ...args) {
  return FunctionPrototypeCall(fn, thisArg, ...args);
}

/**
 * Invokes the error() handler, or sends the fallback response (Section 16.3).
 * @param {object} handlers
 * @param {import('./exchange').Exchange} exchange
 * @param {object} ctx
 * @param {any} error
 * @returns {Promise<void>}
 */
async function handleFault(handlers, exchange, ctx, error) {
  const fallback = fallbackStatus(exchange, error);
  if (handlers.error === undefined) {
    report(error);
    return sendFallback(exchange, fallback);
  }
  let result;
  try {
    result = await invoke(handlers.error, handlers.object, ctx, error);
  } catch (err) {
    report(err);
    if (exchange.responseCapable) return sendFallback(exchange, fallback);
    return;
  }
  if (exchange.state === 'aborted') return;
  if (exchange.state === 'denied') {
    if (result !== undefined) report(invalidReturn(result));
    return;
  }
  if (!exchange.responseCapable) return;
  if (result === undefined) return sendFallback(exchange, fallback);
  let response;
  try {
    response = toResponse(exchange, result);
  } catch (err) {
    report(err);
    return sendFallback(exchange, fallback);
  }
  return sendResponse(exchange, response);
}

/**
 * Processes the settlement of a fetch() or connect() invocation (Section 16.4).
 * @param {object} handlers
 * @param {import('./exchange').Exchange} exchange
 * @param {object} ctx
 * @param {boolean} threw
 * @param {any} error
 * @param {any} result
 * @returns {Promise<boolean>} true if fetch() should now be invoked for a
 *   fall-through.
 */
async function settle(handlers, exchange, ctx, threw, error, result) {
  if (exchange.state === 'aborted') {
    if (threw) report(error);
    return false;
  }
  if (exchange.state === 'denied') {
    if (threw) report(error);
    else if (result !== undefined) report(invalidReturn(result));
    return false;
  }
  if (exchange.state === 'committed' || exchange.state === 'complete') {
    // A tunnel has been established.
    if (threw) {
      report(error);
      const tunnel = exchange.tunnel;
      if (tunnel !== null) {
        tunnel.destroy(createProtocolError(
          exchange.isWebSocket ? kRequestCancelled : kConnectError,
          'The connect() handler failed after accepting the tunnel'));
      }
    } else if (result !== undefined) {
      report(invalidReturn(result));
    }
    return false;
  }
  if (threw) {
    await handleFault(handlers, exchange, ctx, error);
    return false;
  }
  if (result === undefined) {
    if (exchange.fallThrough) return true;
    await handleFault(handlers, exchange, ctx,
                      invalidReturn(result));
    return false;
  }
  let response;
  try {
    response = toResponse(exchange, result);
  } catch (err) {
    await handleFault(handlers, exchange, ctx, err);
    return false;
  }
  await sendResponse(exchange, response);
  return false;
}

async function runHandler(handlers, fn, exchange, ctx) {
  let threw = false;
  let error;
  let result;
  try {
    result = await invoke(fn, handlers.object, ctx);
  } catch (err) {
    threw = true;
    error = err;
  }
  return settle(handlers, exchange, ctx, threw, error, result);
}

/**
 * Dispatches an exchange to the application's handlers.
 * @param {object} handlers The converted HandlerObject.
 * @param {import('./exchange').Exchange} exchange
 */
async function dispatch(handlers, exchange) {
  try {
    if (exchange.kind === 'fetch') {
      if (handlers.fetch === undefined) {
        await sendFallback(exchange, 501);
        return;
      }
      await runHandler(handlers, handlers.fetch, exchange,
                       createContext(exchange, false));
      return;
    }
    const fallThrough = await runHandler(handlers, handlers.connect, exchange,
                                         createContext(exchange, true));
    if (!fallThrough || exchange.state === 'aborted') return;
    if (handlers.fetch === undefined) {
      await sendFallback(exchange, 501);
      return;
    }
    exchange.fallThrough = false;
    await runHandler(handlers, handlers.fetch, exchange,
                     createContext(exchange, false));
  } catch (err) {
    // Errors here are implementation errors; make sure the exchange ends.
    report(err);
    if (!exchange.terminal) {
      exchange.backend.reset(kInternalError);
      exchange.complete();
    }
  } finally {
    exchange.handlerDone();
  }
}

module.exports = {
  dispatch,
  sendResponse,
};
