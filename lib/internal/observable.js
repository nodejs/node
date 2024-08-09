const {
  ReflectConstruct,
  Symbol,
  SymbolAsyncIterator,
  SymbolIterator,
} = primordials;

const { EventEmitter } = require('events');

const {
  AbortController,
  AbortSignal,
} = require('internal/abort_controller');

const {
  validateFunction,
  validateObject,
  validateAbortSignal,
} = require('internal/validators');

const {
  codes: {
    ERR_ILLEGAL_INVOCATION,
    ERR_INVALID_ARG_TYPE,
    ERR_METHOD_NOT_IMPLEMENTED,
  },
} = require('internal/errors');

const {
  createDeferredPromise,
} = require('internal/util');

const kClose = Symbol('kClose');
const kSignal = Symbol('kSignal');
const kState = Symbol('kState');
const kObserverMarker = Symbol('kObserverMarker');
const kTeardown = Symbol('kTeardown');

/**
 * @typedef {Object} SubscribeOptions
 * @property {AbortSignal} [signal]
 */

/**
 * @callback SubscriberNextCallback
 * @param {any} value
 * @returns {void}
 */

/**
 * @callback SubscriberErrorCallback
 * @param {Error} error
 * @returns {void}
 */

/**
 * @callback SubscriberCompleteCallback
 * @returns {void}
 */

/**
 * @callback SubscriberCallback
 * @param {Subscriber} subscriber
 */

/**
 * @callback SubscriptionObserverCallback
 * @param {any} value
 * @returns {void}
 */

/**
 * @callback VoidFunction
 * @returns {void}
 */

/**
 * @typedef {Object} SubscriptionObserver
 * @property {SubscriptionObserverCallback} [next]
 * @property {SubscriptionObserverCallback} [error]
 * @property {VoidFunction} [complete]
 */

/**
 * @callback Mapper
 * @param {any} element
 * @param {number} index
 * @returns {any}
 */

/**
 * @callback Predicate
 * @param {any} element
 * @param {number} index
 * @returns {boolean}
 */

/**
 * @callback Reducer
 * @param {any} accumulator
 * @param {any} element
 * @returns {any}
 */

class Subscriber {
  constructor() {
    throw new ERR_ILLEGAL_INVOCATION('Subscriber');
  }

  next(value) { this[kState]?.next(value); }

  complete() { this[kState]?.complete(); }

  error(error) {
    const err = this[kState]?.error;
    this[kClose]();
    if (typeof err === 'function') {
      err(error);
    } else {
      throw error;
      // The spec expects us to use reportError();
    }
  }

  addTeardown(teardown) {
    if (this[kState] === undefined) return;
    this[kState].teardowns.push(teardown);
  }

  get active() { return this[kState] === undefined; }

  get signal() { return this[kSignal]; }

  [kClose]() { this[kState] = undefined; }

  [kTeardown]() {
    if (this[kState] === undefined) return;
    const teardowns = this[kState].teardowns;
    this[kState] = undefined;
    teardowns.reverse();
    for (const teardown of teardowns) {
      teardown();
    }
  }
}

/**
 * @param {SubscribOptions} options
 * @returns {Subscriber}
 */
function createSubscriber(internalObserver, options) {
  validateObject(options, 'options');
  validateObject(internalObserver, 'internalObserver');
  const { signal } = options;
  const { next, error, complete } = internalObserver;
  if (signal !== undefined) {
    validateAbortSignal(signal, 'options.signal');
  }
  return ReflectConstruct(function() {
    this[kState] = {
      completeOrErrorController: new AbortController(),
      teardowns: [],
      next,
      error,
      complete,
    };
    const signals = [
      this[kState].completeOrErrorController.signal
    ];
    if (signal !== undefined) signals.push(signal);
    this[kSignal] = AbortSignal.any(signals);
  }, [], Subscriber);
}

class Observable {
  #subscriberCallback = undefined;

  static from(value) {
    if (value?.[kObserverMarker] === kObserverMarker) return value;

    if (value instanceof Promise) {
      return new Observable((subscriber) => {
        value.then(
          (value) => {
            subscriber.next(value);
            subscriber.complete();
          },
          (error) => subscriber.error(error)
        );
      });
    }

    if (typeof value?.[SymbolAsyncIterator] === 'function' ||
        typeof value?.[SymbolIterator] === 'function') {
      const processIterable = async (asyncIterable, subscriber) => {
        if (!subscriber.active) return;
        for await (const chunk of asyncIterable) {
          if (!subscriber.active) return;
          subscriber.next(chunk);
        }
        subscriber.complete();
      };

      return new Observable((subscriber) => {
        processIterable(value, subscriber)
          .catch((err) => subscriber.error(err));
      });
    }

    throw new ERR_INVALID_ARG_TYPE('value',
      ['Observable', 'Promise', 'AsyncIterable', 'Iterable'], value);
  }

  constructor(subscriberCallback) {
    validateFunction(subscriberCallback, 'subscriberCallback');
    this.#subscriberCallback = subscriberCallback;
    this[kObserverMarker] = kObserverMarker;
  }

  /**
   * @typedef {SubscriptionObserverCallback|SubscriptionObserver} ObserverUnion
   *
   * @param {ObserverUnion} observer
   * @param {SubscribOptions} options
   */
  subscribe(observer = {}, options = {}) {
    let internalObserver = {
      next(value) {},
      error(error) {
        throw error;
        // The spec expects us to use reportError();
      },
      complete() {},
    };
    if (typeof observer === 'function') {
      internalObserver = {
        next(value) {
          try {
            observer(value);
          } catch (err) {
            throw error;
            // The spec expects us to use reportError();
          }
        }
      };
    } else if (typeof observer === 'object' && observer !== null) {
      const { next, error, complete } = observer;
      if (next != null) {
        internalObserver.next = (value) => {
          try {
            next(value);
          } catch (err) {
            throw error;
            // The spec expects us to use reportError();
          }
        };
      }
      if (error != null) {
        internalObserver.error = (err) => {
          try {
            error(err);
          } catch (err) {
            throw error;
            // The spec expects us to use reportError();
          }
        };
      }
      if (complete != null) {
        internalObserver.complete = () => {
          try {
            complete();
          } catch (err) {
            throw error;
            // The spec expects us to use reportError();
          }
        };
      }
    }
    const subscriber = createSubscriber(internalObserver, options);
    if (subscriber.signal.aborted) {
      subscriber[kClose]();
    } else {
      subscriber.signal.addEventListener('abort', () => {
        try {
          subscriber[kTeardown]();
        } catch (err) {
          subscriber.error(err);
        }
      });
    }
    try {
      this.#subscriberCallback(subscriber);
    } catch (err) {
      subscriber.error(err);
    }
  }

  /**
   * @param {*} notifier
   * @returns {Observable}
   */
  takeUntil(notifier) {
    return new Observable((subscriber) => {
      const notifierObservable = Observable.from(notifier);
      notifierObservable.subscribe({
        next() {
          subscriber.complete();
        },
        error(error) {
          subscriber.error(error);
        },
      }, {
        signal: subscriber.signal,
      });
      if (!subscriber.active) return;
      this.subscribe({
        next(value) {
          subscriber.next(value);
        },
        error(error) {
          subscriber.error(error);
        },
        complete() {
          subscriber.complete();
        },
      }, {
        signal: subscriber.signal,
      });
    });
  }

  /**
   * @param {Mapper} mapper
   * @returns {Observable}
   */
  map(mapper) {
    return new Observable((subscriber) => {
      let idx = 0;
      this.subscribe({
        next(value) {
          try {
            subscriber.next(mapper(value, idx++));
          } catch (err) {
            subscriber.error(err);
          }
        },
        error(error) {
          subscriber.error(error);
        },
        complete() {
          subscriber.complete();
        },
      }, {
        signal: subscriber.signal,
      });
    });
  }

  /**
   * @param {Predicate} predicate
   * @returns {Observable}
   */
  filter(predicate) {
    return new Observable((subscriber) => {
      let idx = 0;
      this.subscribe({
        next(value) {
          try {
            if (predicate(value, idx++)) {
              subscriber.next(value);
            }
          } catch (err) {
            subscriber.error(err);
          }
        },
        error(error) {
          subscriber.error(error);
        },
        complete() {
          subscriber.complete();
        },
      }, {
        signal: subscriber.signal,
      });
    });
  }

  /**
   * @param {number} amount
   * @returns {Observable}
   */
  take(amount) {
    return new Observable((subscriber) => {
      if (amount == 0) {
        subscriber.complete();
        return;
      }
      this.subscribe({
        next(value) {
          subscriber.next(value);
          if (--amount === 0) {
            subscriber.complete();
          }
        },
        error(error) {
          subscriber.error(error);
        },
        complete() {
          subscriber.complete();
        },
      }, {
        signal: subscriber.signal,
      });
    });
  }

  /**
   * @param {number} amount
   * @returns {Observable}
   */
  drop(amount) {
    return new Observable((subscriber) => {
      subscriber.subscribe({
        next(value) {
          if (amount > 0) {
            amount--;
            return;
          }
          subscriber.next(value);
        },
        error(error) {
          subscriber.error(error);
        },
        complete() {
          subscriber.complete();
        },
      }, {
        signal: subscriber.signal,
      });
    });  }


  /**
   * @param {Mapper} mapper
   * @returns {Observable}
   */
  flatMap(mapper) {
    return new Observable((subscriber) => {
      let idx = 0;
      let outerSubscriptionHasCompleted = false;
      let activeInnerSubscription = false;
      let queue = [];

      const processFlatMap = (value) => {
        try {
          let mappedResult = mapper(value, idx++);
          let innerObservable = Observable.from(mappedResult);
          innerObservable.subscribe({
            next(value) { subscriber.next(value); },
            error(error) { subscriber.error(error); },
            complete() {
              if (queue.length > 0) {
                return processFlatMap(queue.shift());
              }
              activeInnerSubscription = false;
              if (outerSubscriptionHasCompleted) subscriber.complete();
            },
          }, {
            signal: subscriber.signal,
          });
        } catch (err) {
          subscriber.error(err);
        }
      };

      this.subscribe({
        next(value) {
          if (activeInnerSubscription) {
            queue.push(value);
            return;
          }
          activeInnerSubscription = true;
          processFlatMap(value);
        },
        error(error) {
          subscriber.error(error);
        },
        complete() {
          outerSubscriptionHasCompleted = true;
          if (!activeInnerSubscription && queue.length === 0) {
            subscriber.complete();
          }
        },
      }, {
        signal: subscriber.signal,
      });
    });
  }

  /**
   * @param {Mapper} mapper
   * @returns {Observable}
   */
  switchMap(mapper) {
    return new Observable((subscriber) => {
      let idx = 0;
      let outerSubscriptionHasCompleted = false;
      let activeInnerAbortController = null;

      const processSwitchMap = (value) => {
        try {
          let mappedResult = mapper(value, idx++);
          let innerObservable = Observable.from(mappedResult);
          innerObservable.subscribe({
            next(value) { subscriber.next(value); },
            error(error) { subscriber.error(error); },
            complete() {
              if (outerSubscriptionHasCompleted) {
                return subscriber.complete();
              }
              activeInnerAbortController = null;
            }
          }, {
            signal: AbortSignal.any([
              activeInnerAbortController.signal,
              subscriber.signal,
            ]),
          });
        } catch (err) {
          subscriber.error(err);
        }
      };

      subscriber.subscribe({
        next(value) {
          if (activeInnerAbortController !== null) {
            activeInnerAbortController.abort();
          }
          activeInnerAbortController = new AbortController();
          processSwitchMap(value);
        },
        error(error) { subscriber.error(error); },
        complete() {
          outerSubscriptionHasCompleted = true;
          if (activeInnerAbortController == null) subscriber.complete();
        },
      }, {
        signal: subscriber.signal,
      });
    });
  }

  /**
   * @param {VoidCallback} callback
   * @returns {Observable}
   */
  finally(callback) {
    throw new ERR_METHOD_NOT_IMPLEMENTED('Observable.prototype.finally');
  }

  /**
   * @param {SubcribeOptions} SubscribeOptions
   * @param {*} options
   * @returns {Promise<any[]>}
   */
  toArray(options = {}) {
    const { promise, resolve, reject } = createDeferredPromise();
    const signal = options?.signal;
    if (signal != null) {
      validateAbortSignal(options.signal, 'options.signal');
      if (signal.aborted) {
        reject(signal.reason);
        return promise;
      }
      signal.addEventListener('abort', () => {
        reject(signal.reason);
      });
    }
    const values = [];
    this.subscribe({
      next(value) { values.push(value); },
      error(error) { reject(error); },
      complete() { resolve(values); },
    }, options);
    return promise;
  }

  /**
   * @callback Visitor
   * @param {any} element
   * @param {number} index
   * @returns {void}
   *
   * @param {Visitor} callback
   * @param {SubscribeOptions} options
   * @returns {Promise<void>}
   */
  forEach(callback, options = {}) {
    const { promise, resolve, reject } = createDeferredPromise();
    const visitorCallbackController = new AbortController();
    const signals = [ visitorCallbackController.signal ];
    if (options?.signal != null) {
      validateAbortSignal(options.signal, 'options.signal');
      signals.push(options.signal);
    }
    const internalOptions = {
      signal: AbortSignal.any(signals),
    };
    if (internalOptions.signal.aborted) {
      reject(internalOptions.signal.reason);
      return promise;
    }
    internalOptions.signal.addEventListener('abort', () => {
      reject(internalOptions.signal.reason);
    });
    let idx = 0;
    this.subscribe({
      next(value) {
        try {
          callback(value, idx++);
        } catch (err) {
          reject(err);
          visitorCallbackController.abort(err);
        }
      },
      error(error) { reject(error); },
      complete() { resolve(); },
    }, internalOptions);
    return promise;
  }

  /**
   * @param {Predicate} predicate
   * @param {SubscribeOptions} options
   * @returns {Promise<boolean>}
   */
  every(predicate, options = {}) {
    const { promise, resolve, reject } = createDeferredPromise();
    const controller = new AbortController();
    const signals = [controller.signal];
    if (options?.signal != null) {
      validateAbortSignal(options.signal, 'options.signal');
      signals.push(options.signal);
    }
    const internalOptions = {
      signal: AbortSignal.any(signals),
    };
    if (internalOptions.signal.aborted) {
      reject(internalOptions.signal.reason);
      return promise;
    }
    internalOptions.signal.addEventListener('abort', () => {
      reject(internalOptions.signal.reason);
    });
    let idx = 0;
    this.subscribe({
      next(value) {
        try {
          const passed = predicate(value, idx++);
          if (!passed) {
            resolve(false);
            controller.abort();
          }
        } catch (err) {
          reject(err);
          controller.abort(err);
        }
      },
      error(error) { reject(error); },
      complete() { resolve(true); },
    }, internalOptions);
    return promise;
  }

  /**
   * @param {SubscribeOptions} options
   * @returns {Promise<any>}
   */
  first(options = {}) {
    const { promise, resolve, reject } = createDeferredPromise();
    const controller = new AbortController();
    const signals = [controller.signal];
    if (options?.signal != null) {
      validateAbortSignal(options.signal, 'options.signal');
      signals.push(options.signal);
    }
    const internalOptions = {
      signal: AbortSignal.any(signals),
    };
    if (internalOptions.signal.aborted) {
      reject(internalOptions.signal.reason);
      return promise;
    }
    internalOptions.signal.addEventListener('abort', () => {
      reject(internalOptions.signal.reason);
    });
    this.subscribe({
      next(value) {
        resolve(p);
        controller.abort();
      },
      error(error) { reject(error); },
      complete() { resolve(); },
    }, internalOptions);
    return promise;
  }

  /**
   * @param {SubscribeOptions} options
   * @returns {Promise<any>}
   */
  last(options = {}) {
    const { promise, resolve, reject } = createDeferredPromise();
    const signal = options?.signal;
    if (signal != null) {
      validateAbortSignal(signal, 'options.signal');
      if (signal.aborted) {
        reject(signal.reason);
        return promise;
      }
      signal.addEventListener('abort', () => {
        reject(signal.reason);
      });
    }
    let lastValue;
    let hasLastValue = false;
    this.subscribe({
      next(value) {
        lastValue = value;
        hasLastValue = true;
      },
      error(error) { reject(error); },
      complete() {
        if (hasLastValue) resolve(lastValue);
        else resolve(undefined);
      }
    }, options);
    return promise;
  }

  /**
   * @param {Predicate} predicate
   * @param {SubscribeOptions} options
   * @returns {Promise<any>}
   */
  find(predicate, options = {}) {
    const { promise, resolve, reject } = createDeferredPromise();
    const controller = new AbortController();
    const signals = [controller.signal];
    if (options?.signal != null) {
      validateAbortSignal(options.signal, 'options.signal');
      signals.push(options.signal);
    }
    const internalOptions = {
      signal: AbortSignal.any(signals),
    };
    if (internalOptions.signal.aborted) {
      reject(internalOptions.signal.reason);
      return promise;
    }
    internalOptions.signal.addEventListener('abort', () => {
      reject(internalOptions.signal.reason);
    });
    let idx = 0;
    this.subscribe({
      next(value) {
        try {
          if (predicate(value, idx++)) {
            resolve(value);
            controller.abort();
          }
        } catch (err) {
          reject(err);
          controller.abort(err);
        }
      },
      error(error) { reject(error); },
      complete() { resolve(undefined); },
    }, internalOptions);
    return promise;
  }

  /**
   * @param {Predicate} predicate
   * @param {SubscribeOptions} options
   * @returns {Promise<boolean>}
   */
  some(predicate, options = {}) {
    const { promise, resolve, reject } = createDeferredPromise();
    const controller = new AbortController();
    const signals = [controller.signal];
    if (options?.signal != null) {
      validateAbortSignal(options.signal, 'options.signal');
      signals.push(options.signal);
    }
    const internalOptions = {
      signal: AbortSignal.any(signals),
    };
    if (internalOptions.signal.aborted) {
      reject(internalOptions.signal.reason);
      return promise;
    }
    internalOptions.signal.addEventListener('abort', () => {
      reject(internalOptions.signal.reason);
    });
    let idx = 0;
    this.subscribe({
      next(value) {
        try {
          if (predicate(value, idx++)) {
            resolve(true);
            controller.abort();
          }
        } catch (err) {
          reject(err);
          controller.abort(err);
        }
      },
      error(error) { reject(error); },
      complete() { resolve(false); },
    }, internalOptions);
    return promise;
  }

  /**
   * @param {Reducer} reducer
   * @param {any} initialValue
   * @param {SubscribeOptions} options
   * @returns {Promise<any>}
   */
  reduce(reducer, initialValue = undefined, options = {}) {
    const { promise, resolve, reject } = createDeferredPromise();
    const controller = new AbortController();
    const signals = [controller.signal];
    if (options?.signal != null) {
      validateAbortSignal(options.signal, 'options.signal');
      signals.push(options.signal);
    }
    const internalOptions = {
      signal: AbortSignal.any(signals),
    };
    if (internalOptions.signal.aborted) {
      reject(internalOptions.signal.reason);
      return promise;
    }
    internalOptions.signal.addEventListener('abort', () => {
      reject(internalOptions.signal.reason);
    });
    let accumulator = initialValue;
    this.subscribe({
      next(value) {
        try {
          accumulator = reducer(accumulator, value);
        } catch (err) {
          reject(err);
          controller.abort(err);
        }
      },
      error(error) { reject(error); },
      complete() { resolve(accumulator); },
    }, internalOptions);
    return promise;
  }
}

/**
 * @param {WeakRef<EventTarget>} eventTargetWeakRef
 */
function getObservableFromEventTarget(event, options, eventTargetWeakRef) {
  return new Observable((subscriber) => {
    const eventTarget = eventTargetWeakRef.deref();
    if (eventTarget == null || subscriber.signal.aborted) return;
    eventTarget.addEventListener(event,
      (event) => subscriber.next(event), {
        capture: options?.capture,
        passive: options?.passive,
      });
  });
}

/**
 * @param {WeakRef<EventEmitter>} eventEmitterWeakRef
 */
function getObservableFromEventEmitter(event, eventEmitterWeakRef) {
  return new Observable((subscriber) => {
    const emitter = eventEmitterWeakRef.deref();
    if (emitter == null || subscriber.signal.aborted) return;
    emitter.on(event, (event) => subscriber.next(event));
    emitter.on('error', (error) => subscriber.error(error));
  });
}

module.exports = {
  Subscriber,
  Observable,
  getObservableFromEventTarget,
  getObservableFromEventEmitter,
};
