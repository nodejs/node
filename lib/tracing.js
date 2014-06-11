// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var EventEmitter = require('events');
var v8binding, process;

// This needs to be loaded early, and before the "process" object is made
// global. So allow src/node.js to pass the process object in during
// initialization.
exports._nodeInitialization = function nodeInitialization(pobj) {
  process = pobj;
  v8binding = process.binding('v8');

  // Finish setting up the v8 Object.
  v8.getHeapStatistics = v8binding.getHeapStatistics;

  // Part of the AsyncListener setup to share objects/callbacks with the
  // native layer.
  process._setupAsyncListener(asyncFlags,
                              runAsyncQueue,
                              loadAsyncQueue,
                              unloadAsyncQueue);

  // Do a little housekeeping.
  delete exports._nodeInitialization;
};


// v8

var v8 = exports.v8 = new EventEmitter();


function emitGC(before, after) {
  v8.emit('gc', before, after);
}


v8.on('newListener', function(name) {
  if (name === 'gc' && EventEmitter.listenerCount(this, name) === 0) {
    v8binding.startGarbageCollectionTracking(emitGC);
  }
});


v8.on('removeListener', function(name) {
  if (name === 'gc' && EventEmitter.listenerCount(this, name) === 0) {
    v8binding.stopGarbageCollectionTracking();
  }
});


// AsyncListener

// new Array() is used here because it is more efficient for sparse
// arrays. Please *do not* change these to simple bracket notation.

// Track the active queue of AsyncListeners that have been added.
var asyncQueue = new Array();

// Keep the stack of all contexts that have been loaded in the
// execution chain of asynchronous events.
var contextStack = new Array();
var currentContext = undefined;

// Incremental uid for new AsyncListener instances.
var alUid = 0;

// Stateful flags shared with Environment for quick JS/C++
// communication.
var asyncFlags = {};

// Prevent accidentally suppressed thrown errors from before/after.
var inAsyncTick = false;

// To prevent infinite recursion when an error handler also throws
// flag when an error is currenly being handled.
var inErrorTick = false;

// Needs to be the same as src/env.h
var kHasListener = 0;

// Flags to determine what async listeners are available.
var HAS_CREATE_AL = 1 << 0;
var HAS_BEFORE_AL = 1 << 1;
var HAS_AFTER_AL = 1 << 2;
var HAS_ERROR_AL = 1 << 3;

// _errorHandler is scoped so it's also accessible by _fatalException.
exports._errorHandler = errorHandler;

// Needs to be accessible from lib/timers.js so they know when async
// listeners are currently in queue. They'll be cleaned up once
// references there are made.
exports._asyncFlags = asyncFlags;
exports._runAsyncQueue = runAsyncQueue;
exports._loadAsyncQueue = loadAsyncQueue;
exports._unloadAsyncQueue = unloadAsyncQueue;

// Public API.
exports.createAsyncListener = createAsyncListener;
exports.addAsyncListener = addAsyncListener;
exports.removeAsyncListener = removeAsyncListener;

// Load the currently executing context as the current context, and
// create a new asyncQueue that can receive any added queue items
// during the executing of the callback.
function loadContext(ctx) {
  contextStack.push(currentContext);
  currentContext = ctx;

  asyncFlags[kHasListener] = 1;
}

function unloadContext() {
  currentContext = contextStack.pop();

  if (currentContext === undefined && asyncQueue.length === 0)
    asyncFlags[kHasListener] = 0;
}

// Run all the async listeners attached when an asynchronous event is
// instantiated.
function runAsyncQueue(context) {
  var queue = new Array();
  var data = new Array();
  var ccQueue, i, queueItem, value;

  context._asyncQueue = queue;
  context._asyncData = data;
  context._asyncFlags = 0;

  inAsyncTick = true;

  // First run through all callbacks in the currentContext. These may
  // add new AsyncListeners to the asyncQueue during execution. Hence
  // why they need to be evaluated first.
  if (currentContext) {
    ccQueue = currentContext._asyncQueue;
    context._asyncFlags |= currentContext._asyncFlags;
    for (i = 0; i < ccQueue.length; i++) {
      queueItem = ccQueue[i];
      queue[queue.length] = queueItem;
      if ((queueItem.callback_flags & HAS_CREATE_AL) === 0) {
        data[queueItem.uid] = queueItem.data;
        continue;
      }
      value = queueItem.create(queueItem.data);
      data[queueItem.uid] = (value === undefined) ? queueItem.data : value;
    }
  }

  // Then run through all items in the asyncQueue
  if (asyncQueue) {
    for (i = 0; i < asyncQueue.length; i++) {
      queueItem = asyncQueue[i];
      // Quick way to check if an AL instance with the same uid was
      // already run from currentContext.
      if (data[queueItem.uid] !== undefined)
        continue;
      queue[queue.length] = queueItem;
      context._asyncFlags |= queueItem.callback_flags;
      if ((queueItem.callback_flags & HAS_CREATE_AL) === 0) {
        data[queueItem.uid] = queueItem.data;
        continue;
      }
      value = queueItem.create(queueItem.data);
      data[queueItem.uid] = (value === undefined) ? queueItem.data : value;
    }
  }

  inAsyncTick = false;
}

// Load the AsyncListener queue attached to context and run all
// "before" callbacks, if they exist.
function loadAsyncQueue(context) {
  loadContext(context);

  if ((context._asyncFlags & HAS_BEFORE_AL) === 0)
    return;

  var queue = context._asyncQueue;
  var data = context._asyncData;
  var i, queueItem;

  inAsyncTick = true;
  for (i = 0; i < queue.length; i++) {
    queueItem = queue[i];
    if ((queueItem.callback_flags & HAS_BEFORE_AL) > 0)
      queueItem.before(context, data[queueItem.uid]);
  }
  inAsyncTick = false;
}

// Unload the AsyncListener queue attached to context and run all
// "after" callbacks, if they exist.
function unloadAsyncQueue(context) {
  if ((context._asyncFlags & HAS_AFTER_AL) === 0) {
    unloadContext();
    return;
  }

  var queue = context._asyncQueue;
  var data = context._asyncData;
  var i, queueItem;

  inAsyncTick = true;
  for (i = 0; i < queue.length; i++) {
    queueItem = queue[i];
    if ((queueItem.callback_flags & HAS_AFTER_AL) > 0)
      queueItem.after(context, data[queueItem.uid]);
  }
  inAsyncTick = false;

  unloadContext();
}

// Handle errors that are thrown while in the context of an
// AsyncListener. If an error is thrown from an AsyncListener
// callback error handlers will be called once more to report
// the error, then the application will die forcefully.
function errorHandler(er) {
  if (inErrorTick)
    return false;

  var handled = false;
  var i, queueItem, threw;

  inErrorTick = true;

  // First process error callbacks from the current context.
  if (currentContext && (currentContext._asyncFlags & HAS_ERROR_AL) > 0) {
    var queue = currentContext._asyncQueue;
    var data = currentContext._asyncData;
    for (i = 0; i < queue.length; i++) {
      queueItem = queue[i];
      if ((queueItem.callback_flags & HAS_ERROR_AL) === 0)
        continue;
      try {
        threw = true;
        // While it would be possible to pass in currentContext, if
        // the error is thrown from the "create" callback then there's
        // a chance the object hasn't been fully constructed.
        handled = queueItem.error(data[queueItem.uid], er) || handled;
        threw = false;
      } finally {
        // If the error callback thew then die quickly. Only allow the
        // exit events to be processed.
        if (threw) {
          process._exiting = true;
          process.emit('exit', 1);
        }
      }
    }
  }

  // Now process callbacks from any existing queue.
  if (asyncQueue) {
    for (i = 0; i < asyncQueue.length; i++) {
      queueItem = asyncQueue[i];
      if ((queueItem.callback_flags & HAS_ERROR_AL) === 0 ||
          (data && data[queueItem.uid] !== undefined))
        continue;
      try {
        threw = true;
        handled = queueItem.error(queueItem.data, er) || handled;
        threw = false;
      } finally {
        // If the error callback thew then die quickly. Only allow the
        // exit events to be processed.
        if (threw) {
          process._exiting = true;
          process.emit('exit', 1);
        }
      }
    }
  }

  inErrorTick = false;

  unloadContext();

  // TODO(trevnorris): If the error was handled, should the after callbacks
  // be fired anyways?

  return handled && !inAsyncTick;
}

// Instance function of an AsyncListener object.
function AsyncListenerInst(callbacks, data) {
  if (typeof callbacks.create === 'function') {
    this.create = callbacks.create;
    this.callback_flags |= HAS_CREATE_AL;
  }
  if (typeof callbacks.before === 'function') {
    this.before = callbacks.before;
    this.callback_flags |= HAS_BEFORE_AL;
  }
  if (typeof callbacks.after === 'function') {
    this.after = callbacks.after;
    this.callback_flags |= HAS_AFTER_AL;
  }
  if (typeof callbacks.error === 'function') {
    this.error = callbacks.error;
    this.callback_flags |= HAS_ERROR_AL;
  }

  this.uid = ++alUid;
  this.data = data === undefined ? null : data;
}
AsyncListenerInst.prototype.create = undefined;
AsyncListenerInst.prototype.before = undefined;
AsyncListenerInst.prototype.after = undefined;
AsyncListenerInst.prototype.error = undefined;
AsyncListenerInst.prototype.data = undefined;
AsyncListenerInst.prototype.uid = 0;
AsyncListenerInst.prototype.callback_flags = 0;

// Create new async listener object. Useful when instantiating a new
// object and want the listener instance, but not add it to the stack.
// If an existing AsyncListenerInst is passed then any new "data" is
// ignored.
function createAsyncListener(callbacks, data) {
  if (typeof callbacks !== 'object' || callbacks == null)
    throw new TypeError('callbacks argument must be an object');

  if (callbacks instanceof AsyncListenerInst)
    return callbacks;
  else
    return new AsyncListenerInst(callbacks, data);
}

// Add a listener to the current queue.
function addAsyncListener(callbacks, data) {
  // Fast track if a new AsyncListenerInst has to be created.
  if (!(callbacks instanceof AsyncListenerInst)) {
    callbacks = createAsyncListener(callbacks, data);
    asyncQueue.push(callbacks);
    asyncFlags[kHasListener] = 1;
    return callbacks;
  }

  var inQueue = false;
  // The asyncQueue will be small. Probably always <= 3 items.
  for (var i = 0; i < asyncQueue.length; i++) {
    if (callbacks === asyncQueue[i]) {
      inQueue = true;
      break;
    }
  }

  // Make sure the callback doesn't already exist in the queue.
  if (!inQueue) {
    asyncQueue.push(callbacks);
    asyncFlags[kHasListener] = 1;
  }

  return callbacks;
}

// Remove listener from the current queue. Though this will not remove
// the listener from the current context. So callback propagation will
// continue.
function removeAsyncListener(obj) {
  for (var i = 0; i < asyncQueue.length; i++) {
    if (obj === asyncQueue[i]) {
      asyncQueue.splice(i, 1);
      break;
    }
  }

  if (asyncQueue.length > 0 || currentContext !== undefined)
    asyncFlags[kHasListener] = 1;
  else
    asyncFlags[kHasListener] = 0;
}
