'use strict';

const {
  ObjectDefineProperties,
  Promise,
  PromiseResolve,
  Symbol,
  SymbolToStringTag,
} = primordials;

const {
  ERR_ILLEGAL_CONSTRUCTOR,
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_ARG_VALUE,
} = require('internal/errors');
const { lazyDOMException } = require('internal/util');
const {
  validateAbortSignal,
  validateDictionary,
  validateFunction,
  validateString,
} = require('internal/validators');
const { threadId } = require('internal/worker');

const locks = internalBinding('locks');

const kName = Symbol('kName');
const kMode = Symbol('kMode');
const kConstructLockManager = Symbol('kConstructLockManager');

// https://w3c.github.io/web-locks/#api-lock
class Lock {
  constructor(...args) {
    if (!args.length) {
      throw new ERR_ILLEGAL_CONSTRUCTOR();
    }
    this[kName] = args[0];
    this[kMode] = args[1];
  }

  get name() {
    if (this instanceof Lock) {
      return this[kName];
    }
    throw new ERR_INVALID_ARG_TYPE('this', 'Lock', this);
  }

  get mode() {
    if (this instanceof Lock) {
      return this[kMode];
    }
    throw new ERR_INVALID_ARG_TYPE('this', 'Lock', this);
  }
}

ObjectDefineProperties(Lock.prototype, {
  name: { __proto__: null, enumerable: true },
  mode: { __proto__: null, enumerable: true },
  [SymbolToStringTag]: {
    __proto__: null,
    value: 'Lock',
    writable: false,
    enumerable: false,
    configurable: true,
  },
});

// Helper to create Lock objects from internal C++ lock data
function createLock(internalLock) {
  return internalLock === null ? null : new Lock(internalLock.name, internalLock.mode);
}

// Convert LOCK_STOLEN_ERROR to AbortError DOMException
function convertLockError(error) {
  if (error?.message === locks.LOCK_STOLEN_ERROR) {
    return lazyDOMException('The operation was aborted', 'AbortError');
  }
  return error;
}

// https://w3c.github.io/web-locks/#api-lock-manager
class LockManager {
  constructor(...args) {
    if (args[0] !== kConstructLockManager) {
      throw new ERR_ILLEGAL_CONSTRUCTOR();
    }
  }

  /**
   * Request a Web Lock for a named resource.
   * @param {string} name - The name of the lock resource
   * @param {object} [options] - Lock options (optional)
   * @param {string} [options.mode='exclusive'] - Lock mode: 'exclusive' or 'shared'
   * @param {boolean} [options.ifAvailable=false] - Only grant if immediately available
   * @param {boolean} [options.steal=false] - Steal existing locks with same name
   * @param {AbortSignal} [options.signal] - Signal to abort pending lock request
   * @param {Function} callback - Function called when lock is granted
   * @returns {Promise} Promise that resolves when the lock is released
   * @throws {TypeError} When name is not a string or callback is not a function
   * @throws {DOMException} When validation fails or operation is not supported
   */
  // https://w3c.github.io/web-locks/#api-lock-manager-request
  async request(name, options, callback) {
    if (callback === undefined) {
      callback = options;
      options = undefined;
    }

    validateString(name, 'name');
    validateFunction(callback, 'callback');

    if (options !== undefined) {
      validateDictionary(options, 'options');
    }

    // Set default options
    options = {
      mode: locks.LOCK_MODE_EXCLUSIVE,
      ifAvailable: false,
      steal: false,
      signal: undefined,
      ...options,
    };

    if (name.startsWith('-')) {
      // If name starts with U+002D HYPHEN-MINUS (-), then reject promise with a
      // "NotSupportedError" DOMException.
      throw lazyDOMException('Lock name may not start with hyphen',
                             'NotSupportedError');
    }

    if (options.ifAvailable === true && options.steal === true) {
      // If both options' steal dictionary member and option's
      // ifAvailable dictionary member are true, then reject promise with a
      // "NotSupportedError" DOMException.
      throw lazyDOMException('ifAvailable and steal are mutually exclusive',
                             'NotSupportedError');
    }

    if (options.mode !== locks.LOCK_MODE_SHARED && options.mode !== locks.LOCK_MODE_EXCLUSIVE) {
      throw new ERR_INVALID_ARG_VALUE('options.mode',
                                      options.mode,
                                      `must be "${locks.LOCK_MODE_SHARED}" or "${locks.LOCK_MODE_EXCLUSIVE}"`);
    }

    if (options.mode !== locks.LOCK_MODE_EXCLUSIVE && options.steal === true) {
      // If options' steal dictionary member is true and options' mode
      // dictionary member is not "exclusive", then return a promise rejected
      // with a "NotSupportedError" DOMException.
      throw lazyDOMException(`mode: "${locks.LOCK_MODE_SHARED}" and steal are mutually exclusive`,
                             'NotSupportedError');
    }

    if (options.signal &&
      (options.steal === true || options.ifAvailable === true)) {
      // If options' signal dictionary member is present, and either of
      // options' steal dictionary member or options' ifAvailable dictionary
      // member is true, then return a promise rejected with a
      // "NotSupportedError" DOMException.
      throw lazyDOMException('signal cannot be used with steal or ifAvailable',
                             'NotSupportedError');
    }

    if (options.signal !== undefined) {
      validateAbortSignal(options.signal, 'options.signal');
    }

    if (options.signal?.aborted) {
      throw options.signal.reason || lazyDOMException('The operation was aborted', 'AbortError');
    }

    const clientId = `node-${process.pid}-${threadId}`;

    // Handle requests with AbortSignal
    if (options.signal) {
      return new Promise((resolve, reject) => {
        let lockGranted = false;

        const abortListener = () => {
          if (!lockGranted) {
            reject(options.signal.reason ||
              lazyDOMException('The operation was aborted', 'AbortError'));
          }
        };

        options.signal.addEventListener('abort', abortListener, { once: true });

        const wrappedCallback = (lock) => {
          return PromiseResolve().then(() => {
            if (options.signal.aborted) {
              return undefined;
            }
            lockGranted = true;
            return callback(createLock(lock));
          });
        };

        try {
          const released = locks.request(
            name,
            clientId,
            options.mode,
            options.steal,
            options.ifAvailable,
            wrappedCallback,
          );

          // When released promise settles, clean up listener and resolve main promise
          released
            .then(resolve, (error) => reject(convertLockError(error)))
            .finally(() => {
              options.signal.removeEventListener('abort', abortListener);
            });
        } catch (error) {
          options.signal.removeEventListener('abort', abortListener);
          reject(convertLockError(error));
        }
      });
    }

    // When ifAvailable: true and lock is not available, C++ passes null to indicate no lock granted
    const wrapCallback = (internalLock) => {
      const lock = createLock(internalLock);
      return callback(lock);
    };

    // Standard request without signal
    try {
      return await locks.request(name, clientId, options.mode, options.steal, options.ifAvailable, wrapCallback);
    } catch (error) {
      const convertedError = convertLockError(error);
      throw convertedError;
    }
  }

  /**
   * Query the current state of locks for this environment.
   * @returns {Promise<{held: Array<object>, pending: Array<object>}>} Promise resolving to lock manager snapshot
   */
  // https://w3c.github.io/web-locks/#api-lock-manager-query
  async query() {
    if (this instanceof LockManager) {
      return locks.query();
    }
    throw new ERR_INVALID_ARG_TYPE('this', 'LockManager', this);
  }
}

ObjectDefineProperties(LockManager.prototype, {
  request: { __proto__: null, enumerable: true },
  query: { __proto__: null, enumerable: true },
  [SymbolToStringTag]: {
    __proto__: null,
    value: 'LockManager',
    writable: false,
    enumerable: false,
    configurable: true,
  },
});

ObjectDefineProperties(LockManager.prototype.request, {
  length: {
    __proto__: null,
    value: 2,
    writable: false,
    enumerable: false,
    configurable: true,
  },
});

module.exports = {
  Lock,
  LockManager,
  locks: new LockManager(kConstructLockManager),
};
