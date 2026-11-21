'use strict';

// Exchange state model of the Fetch Server API (Section 6), shared by the contexts,
// the request, and the protocol backends.
// https://fetch-server.proposal.wintertc.org/#exchange-states

const {
  ArrayPrototypePush,
  PromisePrototypeThen,
  PromiseReject,
  PromiseResolve,
  PromiseWithResolvers,
  SafeSet,
} = primordials;

const {
  createProtocolError,
  invalidState,
  kRequestBodyRejected,
} = require('internal/http/fetch_server/errors');

const {
  markHandled,
} = require('internal/http/fetch_server/body');

const {
  setUnrefTimeout,
} = require('internal/timers');
const { isError } = require('internal/util');
const { clearTimeout } = require('timers');

let AbortController;
function lazyAbortController() {
  AbortController ??= require('internal/abort_controller').AbortController;
  return AbortController;
}

let reportFn;
/**
 * Reports an error in an implementation-defined manner (Section 7.6, Section 16.4).
 * @param {any} error
 */
function report(error) {
  reportFn ??= (err) => {
    process.emitWarning(isError(err) ? err : `${err}`,
                        'FetchServerWarning');
  };
  reportFn(error);
}

const kTerminalStates = new SafeSet(['complete', 'denied', 'aborted']);

class Exchange {
  // 'open' | 'informational' | 'committed' | 'complete' | 'denied' |
  // 'aborted'
  state = 'open';
  abortReason = undefined;

  /** @type {'fetch'|'connect'|'upgrade'} */
  kind;
  backend;
  listener;
  autoContinue;

  pending100 = false;
  fallThrough = false;
  handlerSettled = false;
  isWebSocket = false;
  // The established tunnel, WebSocket connection, or null.
  tunnel = null;

  #abortController = null;
  #abortWaiters = [];
  #racers = null;
  #completed;
  #waitUntilCount = 0;
  #waitUntilTimer = null;
  #ended = false;
  #onEnded;
  #request = null;
  #requestFactory;
  #bodyRefused = false;

  // Request trailers (Section 8.2).
  trailersState = 'pending';
  trailersList = null;
  trailersError = undefined;
  #trailersPromise = null;
  #trailersSettlers = null;

  constructor(kind, backend, listener, options) {
    this.kind = kind;
    this.backend = backend;
    this.listener = listener;
    this.autoContinue = options.autoContinue;
    this.#onEnded = options.onEnded;
    this.#requestFactory = options.requestFactory;
    this.waitUntilTimeout = options.waitUntilTimeout;
    this.maxRequestBodySize = options.maxRequestBodySize;
    const completed = PromiseWithResolvers();
    markHandled(completed.promise);
    this.#completed = completed;
    if (kind === 'connect') {
      this.failTrailers(invalidState(
        'The request of a CONNECT exchange has no content'));
    }
  }

  get terminal() {
    return kTerminalStates.has(this.state);
  }

  get responseCapable() {
    return this.state === 'open' || this.state === 'informational';
  }

  get completed() {
    return this.#completed.promise;
  }

  get ended() {
    return this.#ended;
  }

  get bodyRefused() {
    return this.#bodyRefused;
  }

  get request() {
    this.#request ??= this.#requestFactory(this);
    return this.#request;
  }

  get signal() {
    if (this.#abortController === null) {
      this.#abortController = new (lazyAbortController())();
      if (this.state === 'aborted') {
        this.#abortController.abort(this.abortReason);
      }
    }
    return this.#abortController.signal;
  }

  /**
   * Registers a callback invoked when the exchange is aborted.
   * @param {(reason: any) => void} fn
   */
  onAbort(fn) {
    if (this.state === 'aborted') {
      fn(this.abortReason);
      return;
    }
    if (this.terminal) return;
    ArrayPrototypePush(this.#abortWaiters, fn);
  }

  /**
   * Races promise against the abort of the exchange: the result settles as
   * promise does, or rejects with the abort reason once the exchange is
   * aborted. A rejection of promise after the abort is not observed.
   *
   * This is cheaper than a promise race, and unlike one it does not add a
   * reaction to a long-lived promise for every call.
   * @param {Promise<any>} promise
   * @returns {Promise<any>}
   */
  raceAbort(promise) {
    if (this.state === 'aborted') {
      markHandled(promise);
      return PromiseReject(this.abortReason);
    }
    const { promise: result, resolve, reject } = PromiseWithResolvers();
    const racers = this.#racers ??= new SafeSet();
    racers.add(reject);
    PromisePrototypeThen(promise, (value) => {
      racers.delete(reject);
      resolve(value);
    }, (error) => {
      racers.delete(reject);
      reject(error);
    });
    return result;
  }

  #clearAbortWaiters() {
    this.#abortWaiters = [];
  }

  /**
   * Transitions to "aborted" (Section 6), propagating the reason to every surface
   * (Section 15.4).
   * @param {any} reason A protocol error.
   */
  abort(reason) {
    if (this.terminal) return;
    this.state = 'aborted';
    this.abortReason = reason;
    this.pending100 = false;
    this.#abortController?.abort(reason);
    this.failTrailers(reason);
    this.#completed.reject(reason);
    const racers = this.#racers;
    if (racers !== null) {
      this.#racers = null;
      for (const reject of racers) reject(reason);
    }
    const waiters = this.#abortWaiters;
    this.#abortWaiters = [];
    for (let i = 0; i < waiters.length; i++) {
      try {
        waiters[i](reason);
      } catch (err) {
        report(err);
      }
    }
    this.#maybeEnd();
  }

  /** Transitions to "complete" (Section 6). */
  complete() {
    if (this.terminal) return;
    this.state = 'complete';
    this.pending100 = false;
    this.#clearAbortWaiters();
    this.#refuseUnreadBody();
    this.#completed.resolve();
    this.#maybeEnd();
  }

  /** Transitions to "denied" (Section 6). */
  deny() {
    this.state = 'denied';
    this.pending100 = false;
    this.#clearAbortWaiters();
    this.#refuseUnreadBody();
    this.#completed.resolve();
    this.#maybeEnd();
  }

  commit() {
    this.state = 'committed';
  }

  #refuseUnreadBody() {
    if (this.trailersState === 'pending') {
      this.refuseBody();
    }
  }

  /**
   * Refuses the remainder of the request body (Section 15.3).
   */
  refuseBody() {
    if (this.#bodyRefused) return;
    this.#bodyRefused = true;
    this.failTrailers(createProtocolError(
      kRequestBodyRejected, 'The request body was not read to its end'));
    this.backend.refuseBody?.();
  }

  // Request trailers (Section 8.2).

  get trailers() {
    if (this.#trailersPromise === null) {
      const { promise, resolve, reject } = PromiseWithResolvers();
      markHandled(promise);
      this.#trailersPromise = promise;
      this.#trailersSettlers = { resolve, reject };
      this.#settleTrailersPromise();
    }
    return this.#trailersPromise;
  }

  #settleTrailersPromise() {
    if (this.#trailersSettlers === null) return;
    if (this.trailersState === 'complete') {
      const { createServerHeaders } =
        require('internal/http/fetch_server/headers');
      this.#trailersSettlers.resolve(
        createServerHeaders(this.trailersList, 'immutable'));
    } else if (this.trailersState === 'errored') {
      this.#trailersSettlers.reject(this.trailersError);
    }
  }

  completeTrailers(list) {
    if (this.trailersState !== 'pending') return;
    this.trailersState = 'complete';
    this.trailersList = list;
    this.#settleTrailersPromise();
  }

  failTrailers(error) {
    if (this.trailersState !== 'pending') return;
    this.trailersState = 'errored';
    this.trailersError = error;
    this.#settleTrailersPromise();
  }

  // Lifecycle (Section 7.6).

  waitUntil(promise) {
    if (this.#ended) return;
    this.#waitUntilCount++;
    PromisePrototypeThen(PromiseResolve(promise), () => {
      this.#waitUntilSettled();
    }, (err) => {
      if (!this.#ended) report(err);
      this.#waitUntilSettled();
    });
  }

  #waitUntilSettled() {
    if (this.#ended) return;
    this.#waitUntilCount--;
    this.#maybeEnd();
  }

  handlerDone() {
    this.handlerSettled = true;
    this.#maybeEnd();
  }

  #maybeEnd() {
    if (this.#ended || !this.terminal || !this.handlerSettled) return;
    if (this.#waitUntilCount > 0) {
      if (this.#waitUntilTimer === null && this.waitUntilTimeout > 0) {
        this.#waitUntilTimer = setUnrefTimeout(() => {
          this.#end();
        }, this.waitUntilTimeout);
      }
      return;
    }
    this.#end();
  }

  #end() {
    if (this.#ended) return;
    this.#ended = true;
    if (this.#waitUntilTimer !== null) {
      clearTimeout(this.#waitUntilTimer);
      this.#waitUntilTimer = null;
    }
    this.#onEnded?.(this);
  }

  /**
   * Ends the exchange without waiting for waitUntil() promises, used by
   * destroy() (Section 18.2.3).
   */
  forceEnd() {
    this.#end();
  }
}

module.exports = {
  Exchange,
  report,
};
