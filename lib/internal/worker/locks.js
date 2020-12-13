/* global SharedArrayBuffer */
'use strict';
const {
  broadcastChannel,
  DOMException
} = internalBinding('messaging');
const {
  threadId,
  isMainThread,
} = require('internal/worker');
const {
  receiveMessageOnPort
} = require('internal/worker/io');
const {
  sticky_message_symbol,
  onclose_message_symbol,
} = internalBinding('symbols');
const assert = require('internal/assert');
const {
  SafeMap,
  SymbolToStringTag,
  Symbol,
  BigUint64Array,
  Int32Array,
  Promise,
  ObjectCreate,
  PromiseResolve,
  ObjectDefineProperties,
  TypeError,
} = primordials;

const kName = Symbol('kName');
const kMode = Symbol('kMode');

// Class that contains all the state around active and pending locks.
class LockInternals {
  constructor() {
    // We identify each thread by the pid + thread id combination.
    this.clientId = `node-${process.pid}-${threadId}`;

    // These maps store the resolve/reject functions for a) when the lock
    // request is granted and b) when the final result of the operation is
    // known.
    this.availableMap = new SafeMap();
    this.resolveMap = new SafeMap();
    this.queueProcessingScheduled = false;

    // Create an internal BroadcastChannel instance for communication with
    // other threads.
    this.channel = broadcastChannel('nodejs-internal:locking');
    this.channel.on('message', (msg) => this.onmessage(msg));
    // Post the message that will be sent when this thread closes down
    // (regardless of how exactly that happens).
    this.channel.postMessage({
      op: 'terminated',
      clientId: this.clientId,
      threadId: threadId,
      [onclose_message_symbol]: true
    });
    // By default, do not keep the event loop running just for locks.
    this.channel.unref();

    // Look for existing messages on the channel. There should be some, unless
    // we are the first thread to set this up.
    this.readSynchronousMessages();
    if (!this.state) {
      // This assertion should be valid because the first Worker instantiation
      // on the main thread makes sure that locks will be initialized.
      assert(isMainThread);
      const sab = new SharedArrayBuffer(16);
      this.state = {
        // Use a shared global counter to get ids for the individual
        // lock requests.
        lockIdCounter: new BigUint64Array(sab, 0, 1),
        // The mutex used to ensure that no two threads access the internal
        // state at the same time. Contains -1 if unlocked, and the locker's
        // threadId otherwise.
        mutexOwner: new Int32Array(sab, 8, 1),
        // Lists of the lock (request)s in their individual states.
        held: [],
        pending: [],
        stolen: [],
      };
      this.unlockMutex();
    }
  }

  readSynchronousMessages() {
    let msg;
    while (msg = receiveMessageOnPort(this.channel)) {
      this.onmessage(msg.message);
    }
  }

  publishState() {
    this.channel.postMessage({
      op: 'stateUpdate',
      state: this.state,
      [sticky_message_symbol]: true
    });
  }

  lockMutex() {
    // Lock the mutex and load all pending messages in order to update the
    // internal state.
    const mutex = this.state.mutexOwner;
    let owner;
    while ((owner = Atomics.compareExchange(mutex, 0, -1, threadId)) !== -1) {
      // We use a timeout to make sure that we still receive messages from
      // threads that have terminated. It should be very rare to actually
      // end up blocked on this, because the first other thread to observe
      // the termination message will use Atomics.notify() to wake us up.
      while (Atomics.wait(mutex, 0, owner, 100) === 'timed-out') {
        this.readSynchronousMessages();
      }
    }
    this.readSynchronousMessages();
  }

  unlockMutex(publish = true) {
    // Provide the updated internal state to the other threads and
    // unlock the mutex.
    if (publish) {
      this.publishState();
    }
    const owner =
      Atomics.compareExchange(this.state.mutexOwner, 0, threadId, -1);
    Atomics.notify(this.state.mutexOwner, 0);
    assert(owner === threadId);
  }

  request(
    { resolve, reject },
    name,
    shared,
    ifAvailable,
    steal) {
    this.lockMutex();

    // Let request be a new lock request (agent, clientId, origin, name,
    // mode, promise).
    const request = {
      name,
      shared,
      clientId: this.clientId,
      id: Atomics.add(this.state.lockIdCounter, 0, 1n)
    };
    let availableResolve, availableReject;
    const available = new Promise((res, rej) => {
      availableResolve = res;
      availableReject = rej;
    });
    this.availableMap.set(request.id, {
      resolve: availableResolve, reject: availableReject
    });
    this.resolveMap.set(request.id, { resolve, reject });

    if (steal) {
      // If steal is true, then run these steps:
      for (let i = 0; i < this.state.held.length;) {
        // For each lock of held:
        // If lock's name is name, then run these steps:
        if (this.state.held[i].name !== name) {
          i++;
          continue;
        }
        this.state.stolen.push(this.state.held[i]);
        // Remove lock from held.
        this.state.held.splice(i, 1);
      }
      // Prepend request in queue.
      this.state.pending.unshift(request);
    } else {
      // If ifAvailable is true and request is not grantable, then enqueue the
      // following steps on callback's relevant settings object's responsible
      // event loop:
      if (ifAvailable && !this.isGrantable(request)) {
        // Let r be the result of invoking callback with null as the
        // only argument.
        this.availableMap.delete(request.id);
        this.resolveMap.delete(request.id);
        this.unlockMutex();
        return null;
      }
      // Enqueue request in queue.
      this.state.pending.push(request);
    }
    // Process the lock request queue queue.
    this.processLockRequestQueue();
    this.unlockMutex();
    // Return request.
    return {
      available,
      abort: () => {
        this.lockMutex();
        // Remove request from queue.
        for (let i = 0; i < this.state.pending.length; i++) {
          if (this.state.pending[i].id === request.id) {
            this.state.pending.splice(i, 1);
            break;
          }
        }
        this.availableMap.get(request.id).resolve();
        this.availableMap.delete(request.id);
        this.resolveMap.delete(request.id);
        // Process the lock request queue queue.
        this.processLockRequestQueue();
        this.unlockMutex();
      },
      release: () => {
        this.lockMutex();
        for (let i = 0; i < this.state.held.length; i++) {
          if (this.state.held[i].id === request.id) {
            this.state.held.splice(i, 1);
            break;
          }
        }
        this.resolveMap.delete(request.id);
        this.processLockRequestQueue();
        this.unlockMutex();
      }
    };
  }

  processLockRequestQueue() {
    let didModify = false;
    let hasOwnPendingRequests = false;
    // For each request of queue:
    for (let i = 0; i < this.state.pending.length;) {
      const request = this.state.pending[i];
      if (request.clientId !== this.clientId) {
        i++;
        continue;
      }
      if (!this.isGrantable(request)) {
        hasOwnPendingRequests = true;
        i++;
        continue;
      }
      // If request is grantable, then run these steps:
      // Append lock to origin's held lock set.
      this.state.pending.splice(i, 1);
      this.state.held.push(request);
      this.availableMap.get(request.id).resolve();
      this.availableMap.delete(request.id);
      didModify = true;
    }
    for (let i = 0; i < this.state.stolen.length;) {
      const request = this.state.stolen[i];
      if (request.clientId !== this.clientId) {
        i++;
        continue;
      }

      // Reject lock's released promise with an "AbortError" DOMException.
      this.resolveMap.get(request.id).reject(
        new DOMException('The operation was aborted', 'AbortError'));
      this.resolveMap.delete(request.id);
      this.state.stolen.splice(i, 1);
      didModify = true;
    }
    // Let the event loop spin in case there are any pending requests pertaining
    // to this thread.
    if (hasOwnPendingRequests) {
      this.channel.ref();
    } else {
      this.channel.unref();
    }
    return didModify;
  }

  isGrantable(request) {
    // A lock request request is said to be grantable if the following steps
    // return true:
    // If mode is "exclusive", then return true if all of the following
    // conditions are true, and false otherwise:
    // - No lock in held has a name that equals name
    // - No lock request in queue earlier than request exists.
    // Otherwise, mode is "shared"; return true if all of the following
    // conditions are true, and false otherwise:
    // - No lock in held has mode "exclusive" and has a name that equals name.
    // - No lock request in queue earlier than request exists.
    for (const lock of this.state.held) {
      if (lock.name === request.name && (!lock.shared || !request.shared)) {
        return false;
      }
    }
    for (const lock of this.state.pending) {
      if (lock.id < request.id &&
          lock.name === request.name && (!lock.shared || !request.shared)) {
        return false;
      }
    }
    return true;
  }

  onmessage(message) {
    switch (message.op) {
      case 'terminated': {
        const { threadId, clientId } = message;
        Atomics.compareExchange(this.state.mutexOwner, 0, threadId, -1);
        Atomics.notify(this.state.mutexOwner, 0);
        queueMicrotask(() => {
          this.lockMutex();
          for (const list of [
            this.state.held, this.state.pending, this.state.stolen
          ]) {
            // For each lock request request with agent equal to the
            // terminating agent:
            // - Abort the request request.
            // For each lock lock with agent equal to the terminating agent:
            // - Release the lock lock.
            for (let i = 0; i < list.length;) {
              if (list[i].clientId === clientId) {
                list.splice(i, 1);
              } else {
                i++;
              }
            }
          }
          this.unlockMutex();
        });
        break;
      }
      case 'stateUpdate': {
        const { state } = message;
        this.state = state;
        if (!this.queueProcessingScheduled) {
          // Enueue a call to processLockRequestQueue().
          this.queueProcessingScheduled = true;
          queueMicrotask(() => {
            this.queueProcessingScheduled = false;
            this.lockMutex();
            const didModify = this.processLockRequestQueue();
            // Only publish the state information again in case we did make
            // a change of some kind to avoid snowballing with state update
            // messages.
            this.unlockMutex(didModify);
          });
        }
        break;
      }
      default:
        assert(false);
    }
  }

  snapshot() {
    this.readSynchronousMessages();
    const printLock = ({ name, shared, clientId }) =>
      ({ name, mode: shared ? 'shared' : 'exclusive', clientId });
    return {
      held: this.state.held.map(printLock),
      pending: this.state.pending.map(printLock),
    };
  }
}
let locks;

// https://wicg.github.io/web-locks/#api-lock
class Lock {
  constructor() {
    // eslint-disable-next-line no-restricted-syntax
    throw new TypeError('Illegal constructor');
  }

  get name() {
    return this[kName];
  }

  get mode() {
    return this[kMode];
  }
}

ObjectDefineProperties(Lock.prototype, {
  name: { enumerable: true },
  mode: { enumerable: true },
  [SymbolToStringTag]: {
    value: 'Lock',
    writable: false,
    enumerable: false,
    configurable: true,
  },
});

// https://wicg.github.io/web-locks/#api-lock-manager
class LockManager {
  constructor() {
    // eslint-disable-next-line no-restricted-syntax
    throw new TypeError('Illegal constructor');
  }

  // https://wicg.github.io/web-locks/#api-lock-manager-request
  async request(name, options, callback) {
    if (!locks) locks = new LockInternals();

    if (callback === undefined) {
      callback = options;
      options = undefined;
    }

    // If options was not passed, then let options be a new LockOptions
    // dictionary with default members.
    options = {
      mode: 'exclusive',
      ifAvailable: false,
      steal: false,
      signal: undefined,
      ...options
    };

    if (name.startsWith('-')) {
      // If name starts with U+002D HYPHEN-MINUS (-), then reject promise with a
      // "NotSupportedError" DOMException.
      throw new DOMException('NotSupportedError');
    } else if (options.ifAvailable === true && options.steal === true) {
      // Otherwise, if both options' steal dictionary member and option's
      // ifAvailable dictionary member are true, then reject promise with a
      // "NotSupportedError" DOMException.
      throw new DOMException('NotSupportedError');
    } else if (options.steal === true && options.mode !== 'exclusive') {
      // Otherwise, if options' steal dictionary member is true and option's
      // mode dictionary member is not "exclusive", then reject promise with a
      // "NotSupportedError" DOMException.
      throw new DOMException('NotSupportedError');
    } else if (options.signal &&
               (options.steal === true || options.ifAvailable === true)) {
      // If options' signal dictionary member is present, and either of
      // options' steal dictionary member or options' ifAvailable dictionary
      // member is true, then return a promise rejected with a
      // "NotSupportedError" DOMException.
      throw new DOMException('NotSupportedError');
    } else if (options.signal && options.signal.aborted) {
      throw new DOMException('The operation was aborted', 'AbortError');
    }

    // Let request be the result of running the steps to request a lock with
    // promise, the current agent, environment's id, origin, callback, name,
    // options' mode dictionary member, options' ifAvailable dictionary
    // member, and option's steal dictionary member.

    let resolve, reject;
    const result = new Promise((res, rej) => {
      resolve = res;
      reject = rej;
    });
    const shared = options.mode === 'shared';
    const waiting = locks.request(
      { resolve, reject },
      name,
      shared,
      options.ifAvailable || false,
      options.steal || false);

    if (!waiting) {
      assert(options.ifAvailable);
      return callback(null);
    }

    const onabort = () => {
      reject(new DOMException('The operation was aborted', 'AbortError'));
      waiting.abort();
    };
    if (options.signal) {
      options.signal.addEventListener('abort', onabort);
    }

    await waiting.available;

    if (options.signal) {
      options.signal.removeEventListener('abort', onabort);
      if (options.signal.aborted) {
        return result;
      }
    }

    const lock = ObjectCreate(Lock.prototype, {
      [kName]: {
        value: name,
        writable: false,
        enumerable: false,
        configurable: false,
      },
      [kMode]: {
        value: shared ? 'shared' : 'exclusive',
        writable: false,
        enumerable: false,
        configurable: false,
      },
    });

    try {
      // This is different from returning the awaited value, because using
      // steal: true can be used by another thread to call reject() on the
      // returned Promise.
      resolve(await callback(lock));
    } finally {
      waiting.release();
    }
    return result;
  }

  // https://wicg.github.io/web-locks/#api-lock-manager-query
  query() {
    if (!locks) locks = new LockInternals();

    return PromiseResolve().then(() => locks.snapshot());
  }
}

ObjectDefineProperties(LockManager.prototype, {
  request: { enumerable: true },
  query: { enumerable: true },
  [SymbolToStringTag]: {
    value: 'LockManager',
    writable: false,
    enumerable: false,
    configurable: true,
  },
});

module.exports = {
  LockManager,
  locks: ObjectCreate(LockManager.prototype)
};
