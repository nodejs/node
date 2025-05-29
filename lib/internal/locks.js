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
  ERR_INVALID_THIS,
} = require('internal/errors');
const {
  kEmptyObject,
  lazyDOMException,
} = require('internal/util');
const {
  validateAbortSignal,
  validateFunction,
} = require('internal/validators');
const { threadId } = require('internal/worker');
const {
  converters,
  createEnumConverter,
  createDictionaryConverter,
} = require('internal/webidl');

const locks = internalBinding('locks');

const kName = Symbol('kName');
const kMode = Symbol('kMode');
const kConstructLock = Symbol('kConstructLock');
const kConstructLockManager = Symbol('kConstructLockManager');

// WebIDL dictionary LockOptions
const convertLockOptions = createDictionaryConverter([
  {
    key: 'mode',
    converter: createEnumConverter('LockMode', [
      'shared',
      'exclusive',
    ]),
    defaultValue: () => 'exclusive',
  },
  {
    key: 'ifAvailable',
    converter: (value) => !!value,
    defaultValue: () => false,
  },
  {
    key: 'steal',
    converter: (value) => !!value,
    defaultValue: () => false,
  },
  {
    key: 'signal',
    converter: converters.object,
  },
]);

// https://w3c.github.io/web-locks/#api-lock
class Lock {
  constructor(symbol = undefined, name, mode) {
    if (symbol !== kConstructLock) {
      throw new ERR_ILLEGAL_CONSTRUCTOR();
    }
    this[kName] = name;
    this[kMode] = mode;
  }

  get name() {
    if (this instanceof Lock) {
      return this[kName];
    }
    throw new ERR_INVALID_THIS('Lock');
  }

  get mode() {
    if (this instanceof Lock) {
      return this[kMode];
    }
    throw new ERR_INVALID_THIS('Lock');
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
  return internalLock === null ? null : new Lock(kConstructLock, internalLock.name, internalLock.mode);
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
  constructor(symbol = undefined) {
    if (symbol !== kConstructLockManager) {
      throw new ERR_ILLEGAL_CONSTRUCTOR();
    }
  }

  /**
   * Request a Web Lock for a named resource.
   * @param {string} name - The name of the lock resource
   * @param {object} [options] - Lock options (optional)
   * @param {string} [options.mode] - Lock mode: 'exclusive' or 'shared' default is exclusive
   * @param {boolean} [options.ifAvailable] - Only grant if immediately available
   * @param {boolean} [options.steal] - Steal existing locks with same name
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

    name = converters.DOMString(name);
    validateFunction(callback, 'callback');

    if (options === undefined || typeof options === 'function') {
      options = kEmptyObject;
    }

    // Convert LockOptions dictionary
    options = convertLockOptions(options);

    const { mode, ifAvailable, steal, signal } = options;

    validateAbortSignal(signal, 'options.signal');

    if (signal) {
      signal.throwIfAborted();
    }

    if (name.startsWith('-')) {
      // If name starts with U+002D HYPHEN-MINUS (-), then reject promise with a
      // "NotSupportedError" DOMException.
      throw lazyDOMException('Lock name may not start with hyphen',
                             'NotSupportedError');
    }

    if (ifAvailable === true && steal === true) {
      // If both options' steal dictionary member and option's
      // ifAvailable dictionary member are true, then reject promise with a
      // "NotSupportedError" DOMException.
      throw lazyDOMException('ifAvailable and steal are mutually exclusive',
                             'NotSupportedError');
    }

    if (mode !== locks.LOCK_MODE_EXCLUSIVE && steal === true) {
      // If options' steal dictionary member is true and options' mode
      // dictionary member is not "exclusive", then return a promise rejected
      // with a "NotSupportedError" DOMException.
      throw lazyDOMException(`mode: "${locks.LOCK_MODE_SHARED}" and steal are mutually exclusive`,
                             'NotSupportedError');
    }

    if (signal && (steal === true || ifAvailable === true)) {
      // If options' signal dictionary member is present, and either of
      // options' steal dictionary member or options' ifAvailable dictionary
      // member is true, then return a promise rejected with a
      // "NotSupportedError" DOMException.
      throw lazyDOMException('signal cannot be used with steal or ifAvailable',
                             'NotSupportedError');
    }

    const clientId = `node-${process.pid}-${threadId}`;

    // Handle requests with AbortSignal
    if (signal) {
      return new Promise((resolve, reject) => {
        let lockGranted = false;

        const abortListener = () => {
          if (!lockGranted) {
            reject(signal.reason || lazyDOMException('The operation was aborted', 'AbortError'));
          }
        };

        signal.addEventListener('abort', abortListener, { once: true });

        const wrappedCallback = (lock) => {
          return PromiseResolve().then(() => {
            if (signal.aborted) {
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
            mode,
            steal,
            ifAvailable,
            wrappedCallback,
          );

          // When released promise settles, clean up listener and resolve main promise
          released
            .then(resolve, (error) => reject(convertLockError(error)))
            .finally(() => {
              signal.removeEventListener('abort', abortListener);
            });
        } catch (error) {
          signal.removeEventListener('abort', abortListener);
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
      return await locks.request(name, clientId, mode, steal, ifAvailable, wrapCallback);
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
    throw new ERR_INVALID_THIS('LockManager');
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
