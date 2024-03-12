'use strict';

const {
  ObjectDefineProperty,
  ObjectFromEntries,
  Promise,
  PromiseResolve,
  SafePromiseRace,
  Symbol,
  globalThis,
} = primordials;

const {
  codes: {
    ERR_HTTP_REQUEST_TIMEOUT,
    ERR_INVALID_STATE,
  },
} = require('internal/errors');
const { setTimeout } = require('timers');
const { Readable } = require('internal/streams/readable');
const { URL } = require('url');
const { Event, EventTarget, ExtendableEvent, kTrustEvent } = require('internal/event_target');

const kResponse = Symbol('kResponse');

class FetchEvent extends ExtendableEvent {
  #waitToRespond;
  #respondWithEntered;
  #respondWithError;

  // Handled promise fulfillment
  #resolve;
  #reject;

  constructor(type = 'fetch', fetchOptions = {}) {
    const {
      request,
      preloadResponse,
      clientId,
      resultingClientId,
      replacesClientId,
      ...options
    } = fetchOptions;

    super(type, {
      ...options,
      [kTrustEvent]: true,
    });

    this.#waitToRespond = false;
    this.#respondWithEntered = false;
    this.#respondWithError = false;

    this[kResponse] = undefined;

    ObjectDefineProperty(this, 'handled', {
      __proto__: null,
      value: new Promise((resolve, reject) => {
        this.#resolve = resolve;
        this.#reject = reject;
      }),
    });

    ObjectDefineProperty(this, 'request', {
      __proto__: null,
      value: request,
      enumerable: true,
    });

    ObjectDefineProperty(this, 'preloadResponse', {
      __proto__: null,
      value: PromiseResolve(preloadResponse),
      enumerable: true,
    });

    // Per the spec, client IDs should be passed through or be empty strings.
    ObjectDefineProperty(this, 'clientId', {
      __proto__: null,
      value: clientId || '',
      enumerable: true,
    });
    ObjectDefineProperty(this, 'resultingClientId', {
      __proto__: null,
      value: resultingClientId || '',
      enumerable: true,
    });
    ObjectDefineProperty(this, 'replacesClientId', {
      __proto__: null,
      value: replacesClientId || '',
      enumerable: true,
    });
  }

  respondWith(response) {
    // 1. Let event be this.
    const event = this;

    // 2. If event's dispatch flag is unset, throw an "InvalidStateError"
    // DOMException.
    if (event.eventPhase !== Event.AT_TARGET) {
      throw new ERR_INVALID_STATE('FetchEvent has not been dispatched');
    }

    // 3. If event's respond-with entered flag is set, throw an
    // "InvalidStateError" DOMException.
    if (event.#respondWithEntered) {
      throw new ERR_INVALID_STATE('FetchEvent respond-with already entered');
    }

    // 4. Add lifetime promise r to event.
    const promise = PromiseResolve(response);
    event.waitUntil(promise);

    // 5. Set event's stop propagation flag and stop immediate propagation
    // flag.
    event.stopPropagation();
    event.stopImmediatePropagation();

    // 6. Set event's respond-with entered flag.
    event.#respondWithEntered = true;

    // 7. Set event's wait to respond flag.
    event.#waitToRespond = true;

    // 8. Let targetRealm be event's relevant Realm.
    // NOTE: Nothing to do here...

    // 9. Upon rejection of r:
    promise.catch((err) => {
      // 9.1. Set event's respond-with error flag.
      this.#respondWithError = true;

      // 9.2. Unset event's wait to respond flag.
      this.#waitToRespond = false;

      this.#reject(err);
    });

    // 10. Upon fulfillment of r with response:
    promise.then((response) => {
      // 10.1. If response is not a Response object, then set the respond-with
      // error flag.
      if (!(response instanceof globalThis.Response)) {
        this.#respondWithError = true;

        this.#reject();
      // 10.2. Else:
      } else {
        // NOTE: This deviates from spec as we don't need to reproduce the
        // streaming behaviour without a ServiceWorker in the middle.
        this[kResponse] = response;
        this.#resolve();
      }

      // 10.3. Unset event's wait to respond flag.
      this.#waitToRespond = false;
    });
  }
}

function toWebRequest(req) {
  const url = new URL(req.url, `http://${req.headers.host}`);
  const headers = new globalThis.Headers(req.headers);

  const options = {
    duplex: 'half',
    body: undefined,
    method: req.method,
    headers,
  };

  if (req.method !== 'GET' && req.method !== 'HEAD') {
    options.body = Readable.toWeb(req);
  }

  return new globalThis.Request(url.href, options);
}

function toFetchEvent(req) {
  return new FetchEvent('fetch', {
    request: toWebRequest(req),
  });
}

function makeFetchHandler(handler, timeout = 30000) {
  const target = new EventTarget();
  target.addEventListener('fetch', handler);

  return async function(req, res) {
    // Make FetchEvent instance
    const event = toFetchEvent(req);
    target.dispatchEvent(event);

    await SafePromiseRace([
      event.handled,
      new Promise((_, reject) => {
        const error = new ERR_HTTP_REQUEST_TIMEOUT();
        setTimeout(reject, timeout, error).unref();
      }),
    ]);

    const response = event[kResponse];

    const resHeaders = ObjectFromEntries(response.headers.entries());
    res.writeHead(response.status, resHeaders);

    Readable.fromWeb(response.body).pipe(res);
  };
}

module.exports = {
  makeFetchHandler,
};
