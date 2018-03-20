'use strict';

exports.setup = setupNextTick;

function setupNextTick() {
  const {
    getDefaultTriggerAsyncId,
    newAsyncId,
    initHooksExist,
    destroyHooksExist,
    emitInit,
    emitBefore,
    emitAfter,
    emitDestroy,
    symbols: { async_id_symbol, trigger_async_id_symbol }
  } = require('internal/async_hooks');
  const promises = require('internal/process/promises');
  const { ERR_INVALID_CALLBACK } = require('internal/errors').codes;
  const { emitPromiseRejectionWarnings } = promises;

  // tickInfo is used so that the C++ code in src/node.cc can
  // have easy access to our nextTick state, and avoid unnecessary
  // calls into JS land.
  // runMicrotasks is used to run V8's micro task queue.
  const [
    tickInfo,
    runMicrotasks
  ] = process._setupNextTick(_tickCallback);

  // *Must* match Environment::TickInfo::Fields in src/env.h.
  const kHasScheduled = 0;
  const kHasPromiseRejections = 1;

  // Queue size for each tick array. Must be a power of two.
  const kQueueSize = 2048;
  const kQueueMask = kQueueSize - 1;

  // The next tick queue is implemented as a singly-linked list of fixed-size
  // circular buffers. It looks something like this:
  //
  //  head                                                       tail
  //    |                                                          |
  //    v                                                          v
  // +-----------+ <-----\       +-----------+ <------\         +-----------+
  // |  [null]   |        \----- |   next    |         \------- |   next    |
  // +-----------+               +-----------+                  +-----------+
  // |   tick    | <-- bottom    |   tick    | <-- bottom       |  [empty]  |
  // |   tick    |               |   tick    |                  |  [empty]  |
  // |   tick    |               |   tick    |                  |  [empty]  |
  // |   tick    |               |   tick    |                  |  [empty]  |
  // |   tick    |               |   tick    |       bottom --> |   tick    |
  // |   tick    |               |   tick    |                  |   tick    |
  // |    ...    |               |    ...    |                  |    ...    |
  // |   tick    |               |   tick    |                  |   tick    |
  // |   tick    |               |   tick    |                  |   tick    |
  // |  [empty]  | <-- top       |   tick    |                  |   tick    |
  // |  [empty]  |               |   tick    |                  |   tick    |
  // |  [empty]  |               |   tick    |                  |   tick    |
  // +-----------+               +-----------+ <-- top  top --> +-----------+
  //
  // Or, if there is only one fixed-size queue, it looks something
  // like either of these:
  //
  //  head   tail                                 head   tail
  //    |     |                                     |     |
  //    v     v                                     v     v
  // +-----------+                               +-----------+
  // |  [null]   |                               |  [null]   |
  // +-----------+                               +-----------+
  // |  [empty]  |                               |   tick    |
  // |  [empty]  |                               |   tick    |
  // |   tick    | <-- bottom            top --> |  [empty]  |
  // |   tick    |                               |  [empty]  |
  // |  [empty]  | <-- top            bottom --> |   tick    |
  // |  [empty]  |                               |   tick    |
  // +-----------+                               +-----------+
  //
  // Adding a value means moving `top` forward by one, removing means
  // moving `bottom` forward by one.
  //
  // We let `bottom` and `top` wrap around, so when `top` is conceptual.y
  // pointing to the end of the list, that means that the actual value is `0`.
  //
  // In particular, when `top === bottom`, this can mean *either* that the
  // current queue is empty or that it is full. We can differentiate by
  // checking whether an entry in the queue is empty (a.k.a. `=== undefined`).

  class FixedQueue {
    constructor() {
      this.bottom = 0;
      this.top = 0;
      this.list = new Array(kQueueSize);
      this.next = null;
    }

    push(data) {
      this.list[this.top] = data;
      this.top = (this.top + 1) & kQueueMask;
    }

    shift() {
      const nextItem = this.list[this.bottom];
      if (nextItem === undefined)
        return null;
      this.list[this.bottom] = undefined;
      this.bottom = (this.bottom + 1) & kQueueMask;
      return nextItem;
    }
  }

  var head = new FixedQueue();
  var tail = head;

  function push(data) {
    if (head.bottom === head.top) {
      // Either empty or full:
      if (head.list[head.top] !== undefined) {
        // It's full: Creates a new queue, sets the old queue's `.next` to it,
        // and sets it as the new main queue.
        head = head.next = new FixedQueue();
      } else {
        // If the head is empty, that means that it was the only fixed-sized
        // queue in existence.
        DCHECK_EQ(head.next, null);
        // This is the first tick object in existence, so we need to inform
        // the C++ side that we do want to run `_tickCallback()`.
        tickInfo[kHasScheduled] = 1;
      }
    }
    head.push(data);
  }

  function shift() {
    const next = tail.shift();
    if (tail.top === tail.bottom) { // -> .shift() emptied the current queue.
      if (tail.next !== null) {
        // If there is another queue, it forms the new tail.
        tail = tail.next;
      } else {
        // We've just run out of items. Let the native side know that it
        // doesn't need to bother calling into JS to run the queue.
        tickInfo[kHasScheduled] = 0;
      }
    }
    return next;
  }

  process.nextTick = nextTick;
  // Needs to be accessible from beyond this scope.
  process._tickCallback = _tickCallback;

  function _tickCallback() {
    let tock;
    do {
      while (tock = shift()) {
        const asyncId = tock[async_id_symbol];
        emitBefore(asyncId, tock[trigger_async_id_symbol]);
        // emitDestroy() places the async_id_symbol into an asynchronous queue
        // that calls the destroy callback in the future. It's called before
        // calling tock.callback so destroy will be called even if the callback
        // throws an exception that is handled by 'uncaughtException' or a
        // domain.
        // TODO(trevnorris): This is a bit of a hack. It relies on the fact
        // that nextTick() doesn't allow the event loop to proceed, but if
        // any async hooks are enabled during the callback's execution then
        // this tock's after hook will be called, but not its destroy hook.
        if (destroyHooksExist())
          emitDestroy(asyncId);

        const callback = tock.callback;
        if (tock.args === undefined)
          callback();
        else
          Reflect.apply(callback, undefined, tock.args);

        emitAfter(asyncId);
      }
      runMicrotasks();
    } while (head.top !== head.bottom || emitPromiseRejectionWarnings());
    tickInfo[kHasPromiseRejections] = 0;
  }

  class TickObject {
    constructor(callback, args, triggerAsyncId) {
      // this must be set to null first to avoid function tracking
      // on the hidden class, revisit in V8 versions after 6.2
      this.callback = null;
      this.callback = callback;
      this.args = args;

      const asyncId = newAsyncId();
      this[async_id_symbol] = asyncId;
      this[trigger_async_id_symbol] = triggerAsyncId;

      if (initHooksExist()) {
        emitInit(asyncId,
                 'TickObject',
                 triggerAsyncId,
                 this);
      }
    }
  }

  // `nextTick()` will not enqueue any callback when the process is about to
  // exit since the callback would not have a chance to be executed.
  function nextTick(callback) {
    if (typeof callback !== 'function')
      throw new ERR_INVALID_CALLBACK();

    if (process._exiting)
      return;

    var args;
    switch (arguments.length) {
      case 1: break;
      case 2: args = [arguments[1]]; break;
      case 3: args = [arguments[1], arguments[2]]; break;
      case 4: args = [arguments[1], arguments[2], arguments[3]]; break;
      default:
        args = new Array(arguments.length - 1);
        for (var i = 1; i < arguments.length; i++)
          args[i - 1] = arguments[i];
    }

    push(new TickObject(callback, args, getDefaultTriggerAsyncId()));
  }
}
