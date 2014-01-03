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

// Hello, and welcome to hacking node.js!
//
// This file is invoked by node::Load in src/node.cc, and responsible for
// bootstrapping the node.js core. Special caution is given to the performance
// of the startup process, so many dependencies are invoked lazily.
(function(process) {
  this.global = this;
  var _errorHandler;

  function startup() {
    var EventEmitter = NativeModule.require('events').EventEmitter;

    process.__proto__ = Object.create(EventEmitter.prototype, {
      constructor: {
        value: process.constructor
      }
    });
    EventEmitter.call(process);

    process.EventEmitter = EventEmitter; // process.EventEmitter is deprecated

    // do this good and early, since it handles errors.
    startup.processFatal();

    startup.globalVariables();
    startup.globalTimeouts();
    startup.globalConsole();

    startup.processAsyncListener();
    startup.processAssert();
    startup.processConfig();
    startup.processNextTick();
    startup.processStdio();
    startup.processKillAndExit();
    startup.processSignalHandlers();

    startup.processChannel();

    startup.processRawDebug();

    startup.resolveArgv0();

    // There are various modes that Node can run in. The most common two
    // are running from a script and running the REPL - but there are a few
    // others like the debugger or running --eval arguments. Here we decide
    // which mode we run in.

    if (NativeModule.exists('_third_party_main')) {
      // To allow people to extend Node in different ways, this hook allows
      // one to drop a file lib/_third_party_main.js into the build
      // directory which will be executed instead of Node's normal loading.
      process.nextTick(function() {
        NativeModule.require('_third_party_main');
      });

    } else if (process.argv[1] == 'debug') {
      // Start the debugger agent
      var d = NativeModule.require('_debugger');
      d.start();

    } else if (process._eval != null) {
      // User passed '-e' or '--eval' arguments to Node.
      evalScript('[eval]');
    } else if (process.argv[1]) {
      // make process.argv[1] into a full path
      var path = NativeModule.require('path');
      process.argv[1] = path.resolve(process.argv[1]);

      // If this is a worker in cluster mode, start up the communication
      // channel.
      if (process.env.NODE_UNIQUE_ID) {
        var cluster = NativeModule.require('cluster');
        cluster._setupWorker();

        // Make sure it's not accidentally inherited by child processes.
        delete process.env.NODE_UNIQUE_ID;
      }

      var Module = NativeModule.require('module');

      if (global.v8debug &&
          process.execArgv.some(function(arg) {
            return arg.match(/^--debug-brk(=[0-9]*)?$/);
          })) {

        // XXX Fix this terrible hack!
        //
        // Give the client program a few ticks to connect.
        // Otherwise, there's a race condition where `node debug foo.js`
        // will not be able to connect in time to catch the first
        // breakpoint message on line 1.
        //
        // A better fix would be to somehow get a message from the
        // global.v8debug object about a connection, and runMain when
        // that occurs.  --isaacs

        var debugTimeout = +process.env.NODE_DEBUG_TIMEOUT || 50;
        setTimeout(Module.runMain, debugTimeout);

      } else {
        // Main entry point into most programs:
        Module.runMain();
      }

    } else {
      var Module = NativeModule.require('module');

      // If -i or --interactive were passed, or stdin is a TTY.
      if (process._forceRepl || NativeModule.require('tty').isatty(0)) {
        // REPL
        var opts = {
          useGlobal: true,
          ignoreUndefined: false
        };
        if (parseInt(process.env['NODE_NO_READLINE'], 10)) {
          opts.terminal = false;
        }
        if (parseInt(process.env['NODE_DISABLE_COLORS'], 10)) {
          opts.useColors = false;
        }
        var repl = Module.requireRepl().start(opts);
        repl.on('exit', function() {
          process.exit();
        });

      } else {
        // Read all of stdin - execute it.
        process.stdin.setEncoding('utf8');

        var code = '';
        process.stdin.on('data', function(d) {
          code += d;
        });

        process.stdin.on('end', function() {
          process._eval = code;
          evalScript('[stdin]');
        });
      }
    }
  }

  startup.globalVariables = function() {
    global.process = process;
    global.global = global;
    global.GLOBAL = global;
    global.root = global;
    global.Buffer = NativeModule.require('buffer').Buffer;
    process.domain = null;
    process._exiting = false;
  };

  startup.globalTimeouts = function() {
    global.setTimeout = function() {
      var t = NativeModule.require('timers');
      return t.setTimeout.apply(this, arguments);
    };

    global.setInterval = function() {
      var t = NativeModule.require('timers');
      return t.setInterval.apply(this, arguments);
    };

    global.clearTimeout = function() {
      var t = NativeModule.require('timers');
      return t.clearTimeout.apply(this, arguments);
    };

    global.clearInterval = function() {
      var t = NativeModule.require('timers');
      return t.clearInterval.apply(this, arguments);
    };

    global.setImmediate = function() {
      var t = NativeModule.require('timers');
      return t.setImmediate.apply(this, arguments);
    };

    global.clearImmediate = function() {
      var t = NativeModule.require('timers');
      return t.clearImmediate.apply(this, arguments);
    };
  };

  startup.globalConsole = function() {
    global.__defineGetter__('console', function() {
      return NativeModule.require('console');
    });
  };


  startup._lazyConstants = null;

  startup.lazyConstants = function() {
    if (!startup._lazyConstants) {
      startup._lazyConstants = process.binding('constants');
    }
    return startup._lazyConstants;
  };

  startup.processFatal = function() {
    process._fatalException = function(er) {
      // First run through error handlers from asyncListener.
      var caught = _errorHandler(er);

      if (process.domain && process.domain._errorHandler)
        caught = process.domain._errorHandler(er) || caught;

      if (!caught)
        caught = process.emit('uncaughtException', er);

      // If someone handled it, then great.  otherwise, die in C++ land
      // since that means that we'll exit the process, emit the 'exit' event
      if (!caught) {
        try {
          if (!process._exiting) {
            process._exiting = true;
            process.emit('exit', 1);
          }
        } catch (er) {
          // nothing to be done about it at this point.
        }

      // if we handled an error, then make sure any ticks get processed
      } else {
        var t = setImmediate(process._tickCallback);
        // Complete hack to make sure any errors thrown from async
        // listeners don't cause an infinite loop.
        if (t._asyncQueue)
          t._asyncQueue = [];
      }

      return caught;
    };
  };

  startup.processAsyncListener = function() {
    // new Array() is used here because it is more efficient for sparse
    // arrays. Please *do not* change these to simple bracket notation.

    // Track the active queue of AsyncListeners that have been added.
    var asyncStack = new Array();
    var asyncQueue = undefined;

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
    _errorHandler = errorHandler;

    // Needs to be accessible from lib/timers.js so they know when async
    // listeners are currently in queue. They'll be cleaned up once
    // references there are made.
    process._asyncFlags = asyncFlags;
    process._runAsyncQueue = runAsyncQueue;
    process._loadAsyncQueue = loadAsyncQueue;
    process._unloadAsyncQueue = unloadAsyncQueue;

    // Public API.
    process.createAsyncListener = createAsyncListener;
    process.addAsyncListener = addAsyncListener;
    process.removeAsyncListener = removeAsyncListener;

    // Setup shared objects/callbacks with native layer.
    process._setupAsyncListener(asyncFlags,
                                runAsyncQueue,
                                loadAsyncQueue,
                                unloadAsyncQueue,
                                pushListener,
                                stripListener);

    // Load the currently executing context as the current context, and
    // create a new asyncQueue that can receive any added queue items
    // during the executing of the callback.
    function loadContext(ctx) {
      contextStack.push(currentContext);
      currentContext = ctx;

      asyncStack.push(asyncQueue);
      asyncQueue = new Array();

      asyncFlags[kHasListener] = 1;
    }

    function unloadContext() {
      currentContext = contextStack.pop();
      asyncQueue = asyncStack.pop();

      if (typeof currentContext === 'undefined' &&
          typeof asyncQueue === 'undefined')
        asyncFlags[kHasListener] = 0;
    }

    // Run all the async listeners attached when an asynchronous event is
    // instantiated.
    function runAsyncQueue(context) {
      var queue = new Array();
      var data = new Array();
      var ccQueue, i, item, queueItem, value;

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
          if ((queueItem.flags & HAS_CREATE_AL) === 0) {
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
          queue[queue.length] = queueItem;
          context._asyncFlags |= queueItem.flags;
          if ((queueItem.flags & HAS_CREATE_AL) === 0) {
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
        if ((queueItem.flags & HAS_BEFORE_AL) > 0)
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
        if ((queueItem.flags & HAS_AFTER_AL) > 0)
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
          if ((queueItem.flags & HAS_ERROR_AL) === 0)
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
          if ((queueItem.flags & HAS_ERROR_AL) === 0)
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
        this.flags |= HAS_CREATE_AL;
      }
      if (typeof callbacks.before === 'function') {
        this.before = callbacks.before;
        this.flags |= HAS_BEFORE_AL;
      }
      if (typeof callbacks.after === 'function') {
        this.after = callbacks.after;
        this.flags |= HAS_AFTER_AL;
      }
      if (typeof callbacks.error === 'function') {
        this.error = callbacks.error;
        this.flags |= HAS_ERROR_AL;
      }

      this.uid = ++alUid;
      this.data = data;
    }
    AsyncListenerInst.prototype.create = undefined;
    AsyncListenerInst.prototype.before = undefined;
    AsyncListenerInst.prototype.after = undefined;
    AsyncListenerInst.prototype.error = undefined;
    AsyncListenerInst.prototype.uid = 0;
    AsyncListenerInst.prototype.flags = 0;

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
      if (!asyncQueue) {
        asyncStack.push(asyncQueue);
        asyncQueue = new Array();
      }

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
        if (callbacks.uid === asyncQueue[i].uid) {
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

    // Remove listener from the current queue and the entire stack.
    function removeAsyncListener(obj) {
      var i, j;

      if (asyncQueue) {
        for (i = 0; i < asyncQueue.length; i++) {
          if (obj.uid === asyncQueue[i].uid) {
            asyncQueue.splice(i, 1);
            break;
          }
        }
      }

      // TODO(trevnorris): Why remove the AL from the entire stack?
      for (i = 0; i < asyncStack.length; i++) {
        if (asyncStack[i] === undefined)
          continue;
        for (j = 0; j < asyncStack[i].length; j++) {
          if (obj.uid === asyncStack[i][j].uid) {
            asyncStack[i].splice(j, 1);
            break;
          }
        }
      }

      if ((asyncQueue && asyncQueue.length > 0) ||
          (currentContext && currentContext._asyncQueue.length))
        asyncFlags[kHasListener] = 1;
    }

    // Used by AsyncWrap::AddAsyncListener() to add an individual listener
    // to the async queue. It will check the uid of the listener and only
    // allow it to be added once.
    function pushListener(obj) {
      if (!this._asyncQueue) {
        this._asyncQueue = [obj];
        this._asyncData = new Array();
        this._asyncData[obj.uid] = obj.data;
        this._asyncFlags = obj.flags;
        return;
      }

      if (!this._asyncData)
        this._asyncData = new Array();

      var queue = this._asyncQueue;
      var inQueue = false;
      // The asyncQueue will be small. Probably always <= 3 items.
      for (var i = 0; i < queue.length; i++) {
        if (obj.uid === queue[i].uid) {
          inQueue = true;
          break;
        }
      }

      // Not in the queue so push it on and set the default storage.
      if (!inQueue) {
        queue.push(obj);
        this._asyncData[obj.uid] = obj.data;
        this._asyncFlags |= obj.flags;
      }
    }

    // Used by AsyncWrap::RemoveAsyncListener() to remove an individual
    // listener from the async queue, and return whether there are still
    // listeners in the queue.
    function stripListener(obj) {
      // No queue exists, so nothing to do.
      if (!this._asyncQueue)
        return false;

      var queue = this._asyncQueue;

      // The asyncQueue will be small. Probably always <= 3 items.
      for (var i = 0; i < queue.length; i++) {
        if (obj.uid === queue[i].uid) {
          this._asyncData[queue[i].uid] = undefined;
          queue.splice(i, 1);
          return queue.length > 0;
        }
      }
    }
  };

  var assert;
  startup.processAssert = function() {
    assert = process.assert = function(x, msg) {
      if (!x) throw new Error(msg || 'assertion error');
    };
  };

  startup.processConfig = function() {
    // used for `process.config`, but not a real module
    var config = NativeModule._source.config;
    delete NativeModule._source.config;

    // strip the gyp comment line at the beginning
    config = config.split('\n').slice(1).join('\n').replace(/'/g, '"');

    process.config = JSON.parse(config, function(key, value) {
      if (value === 'true') return true;
      if (value === 'false') return false;
      return value;
    });
  };

  startup.processNextTick = function() {
    var nextTickQueue = [];
    var asyncFlags = process._asyncFlags;
    var _runAsyncQueue = process._runAsyncQueue;
    var _loadAsyncQueue = process._loadAsyncQueue;
    var _unloadAsyncQueue = process._unloadAsyncQueue;

    // This tickInfo thing is used so that the C++ code in src/node.cc
    // can have easy accesss to our nextTick state, and avoid unnecessary
    var tickInfo = {};

    // *Must* match Environment::TickInfo::Fields in src/env.h.
    var kIndex = 0;
    var kLength = 1;

    // For asyncFlags.
    // *Must* match Environment::AsyncListeners::Fields in src/env.h
    var kCount = 0;

    process.nextTick = nextTick;
    // Needs to be accessible from beyond this scope.
    process._tickCallback = _tickCallback;
    process._tickDomainCallback = _tickDomainCallback;

    process._setupNextTick(tickInfo, _tickCallback);

    function tickDone() {
      if (tickInfo[kLength] !== 0) {
        if (tickInfo[kLength] <= tickInfo[kIndex]) {
          nextTickQueue = [];
          tickInfo[kLength] = 0;
        } else {
          nextTickQueue.splice(0, tickInfo[kIndex]);
          tickInfo[kLength] = nextTickQueue.length;
        }
      }
      tickInfo[kIndex] = 0;
    }

    // Run callbacks that have no domain.
    // Using domains will cause this to be overridden.
    function _tickCallback() {
      var callback, hasQueue, threw, tock;

      while (tickInfo[kIndex] < tickInfo[kLength]) {
        tock = nextTickQueue[tickInfo[kIndex]++];
        callback = tock.callback;
        threw = true;
        hasQueue = !!tock._asyncQueue;
        if (hasQueue)
          _loadAsyncQueue(tock);
        try {
          callback();
          threw = false;
        } finally {
          if (threw)
            tickDone();
        }
        if (hasQueue)
          _unloadAsyncQueue(tock);
        if (1e4 < tickInfo[kIndex])
          tickDone();
      }

      tickDone();
    }

    function _tickDomainCallback() {
      var callback, domain, hasQueue, threw, tock;

      while (tickInfo[kIndex] < tickInfo[kLength]) {
        tock = nextTickQueue[tickInfo[kIndex]++];
        callback = tock.callback;
        domain = tock.domain;
        hasQueue = !!tock._asyncQueue;
        if (hasQueue)
          _loadAsyncQueue(tock);
        if (domain)
          domain.enter();
        threw = true;
        try {
          callback();
          threw = false;
        } finally {
          if (threw)
            tickDone();
        }
        if (hasQueue)
          _unloadAsyncQueue(tock);
        if (domain)
          domain.exit();
      }

      tickDone();
    }

    function nextTick(callback) {
      // on the way out, don't bother. it won't get fired anyway.
      if (process._exiting)
        return;

      var obj = {
        callback: callback,
        domain: process.domain || null,
        _asyncQueue: undefined
      };

      if (asyncFlags[kCount] > 0)
        _runAsyncQueue(obj);

      nextTickQueue.push(obj);
      tickInfo[kLength]++;
    }
  };

  function evalScript(name) {
    var Module = NativeModule.require('module');
    var path = NativeModule.require('path');
    var cwd = process.cwd();

    var module = new Module(name);
    module.filename = path.join(cwd, name);
    module.paths = Module._nodeModulePaths(cwd);
    var script = process._eval;
    if (!Module._contextLoad) {
      var body = script;
      script = 'global.__filename = ' + JSON.stringify(name) + ';\n' +
               'global.exports = exports;\n' +
               'global.module = module;\n' +
               'global.__dirname = __dirname;\n' +
               'global.require = require;\n' +
               'return require("vm").runInThisContext(' +
               JSON.stringify(body) + ', { filename: ' +
               JSON.stringify(name) + ' });\n';
    }
    var result = module._compile(script, name + '-wrapper');
    if (process._print_eval) console.log(result);
  }

  function createWritableStdioStream(fd) {
    var stream;
    var tty_wrap = process.binding('tty_wrap');

    // Note stream._type is used for test-module-load-list.js

    switch (tty_wrap.guessHandleType(fd)) {
      case 'TTY':
        var tty = NativeModule.require('tty');
        stream = new tty.WriteStream(fd);
        stream._type = 'tty';

        // Hack to have stream not keep the event loop alive.
        // See https://github.com/joyent/node/issues/1726
        if (stream._handle && stream._handle.unref) {
          stream._handle.unref();
        }
        break;

      case 'FILE':
        var fs = NativeModule.require('fs');
        stream = new fs.SyncWriteStream(fd);
        stream._type = 'fs';
        break;

      case 'PIPE':
      case 'TCP':
        var net = NativeModule.require('net');
        stream = new net.Socket({
          fd: fd,
          readable: false,
          writable: true
        });

        // FIXME Should probably have an option in net.Socket to create a
        // stream from an existing fd which is writable only. But for now
        // we'll just add this hack and set the `readable` member to false.
        // Test: ./node test/fixtures/echo.js < /etc/passwd
        stream.readable = false;
        stream.read = null;
        stream._type = 'pipe';

        // FIXME Hack to have stream not keep the event loop alive.
        // See https://github.com/joyent/node/issues/1726
        if (stream._handle && stream._handle.unref) {
          stream._handle.unref();
        }
        break;

      default:
        // Probably an error on in uv_guess_handle()
        throw new Error('Implement me. Unknown stream file type!');
    }

    // For supporting legacy API we put the FD here.
    stream.fd = fd;

    stream._isStdio = true;

    return stream;
  }

  startup.processStdio = function() {
    var stdin, stdout, stderr;

    process.__defineGetter__('stdout', function() {
      if (stdout) return stdout;
      stdout = createWritableStdioStream(1);
      stdout.destroy = stdout.destroySoon = function(er) {
        er = er || new Error('process.stdout cannot be closed.');
        stdout.emit('error', er);
      };
      if (stdout.isTTY) {
        process.on('SIGWINCH', function() {
          stdout._refreshSize();
        });
      }
      return stdout;
    });

    process.__defineGetter__('stderr', function() {
      if (stderr) return stderr;
      stderr = createWritableStdioStream(2);
      stderr.destroy = stderr.destroySoon = function(er) {
        er = er || new Error('process.stderr cannot be closed.');
        stderr.emit('error', er);
      };
      return stderr;
    });

    process.__defineGetter__('stdin', function() {
      if (stdin) return stdin;

      var tty_wrap = process.binding('tty_wrap');
      var fd = 0;

      switch (tty_wrap.guessHandleType(fd)) {
        case 'TTY':
          var tty = NativeModule.require('tty');
          stdin = new tty.ReadStream(fd, {
            highWaterMark: 0,
            readable: true,
            writable: false
          });
          break;

        case 'FILE':
          var fs = NativeModule.require('fs');
          stdin = new fs.ReadStream(null, { fd: fd });
          break;

        case 'PIPE':
        case 'TCP':
          var net = NativeModule.require('net');
          stdin = new net.Socket({
            fd: fd,
            readable: true,
            writable: false
          });
          break;

        default:
          // Probably an error on in uv_guess_handle()
          throw new Error('Implement me. Unknown stdin file type!');
      }

      // For supporting legacy API we put the FD here.
      stdin.fd = fd;

      // stdin starts out life in a paused state, but node doesn't
      // know yet.  Explicitly to readStop() it to put it in the
      // not-reading state.
      if (stdin._handle && stdin._handle.readStop) {
        stdin._handle.reading = false;
        stdin._readableState.reading = false;
        stdin._handle.readStop();
      }

      // if the user calls stdin.pause(), then we need to stop reading
      // immediately, so that the process can close down.
      stdin.on('pause', function() {
        if (!stdin._handle)
          return;
        stdin._readableState.reading = false;
        stdin._handle.reading = false;
        stdin._handle.readStop();
      });

      return stdin;
    });

    process.openStdin = function() {
      process.stdin.resume();
      return process.stdin;
    };
  };

  startup.processKillAndExit = function() {
    process.exitCode = 0;
    process.exit = function(code) {
      if (code || code === 0)
        process.exitCode = code;

      if (!process._exiting) {
        process._exiting = true;
        process.emit('exit', process.exitCode || 0);
      }
      process.reallyExit(process.exitCode || 0);
    };

    process.kill = function(pid, sig) {
      var err;

      // preserve null signal
      if (0 === sig) {
        err = process._kill(pid, 0);
      } else {
        sig = sig || 'SIGTERM';
        if (startup.lazyConstants()[sig] &&
            sig.slice(0, 3) === 'SIG') {
          err = process._kill(pid, startup.lazyConstants()[sig]);
        } else {
          throw new Error('Unknown signal: ' + sig);
        }
      }

      if (err) {
        var errnoException = NativeModule.require('util')._errnoException;
        throw errnoException(err, 'kill');
      }

      return true;
    };
  };

  startup.processSignalHandlers = function() {
    // Load events module in order to access prototype elements on process like
    // process.addListener.
    var signalWraps = {};
    var addListener = process.addListener;
    var removeListener = process.removeListener;

    function isSignal(event) {
      return event.slice(0, 3) === 'SIG' &&
             startup.lazyConstants().hasOwnProperty(event);
    }

    // Wrap addListener for the special signal types
    process.on = process.addListener = function(type, listener) {
      if (isSignal(type) &&
          !signalWraps.hasOwnProperty(type)) {
        var Signal = process.binding('signal_wrap').Signal;
        var wrap = new Signal();

        wrap.unref();

        wrap.onsignal = function() { process.emit(type); };

        var signum = startup.lazyConstants()[type];
        var err = wrap.start(signum);
        if (err) {
          wrap.close();
          var errnoException = NativeModule.require('util')._errnoException;
          throw errnoException(err, 'uv_signal_start');
        }

        signalWraps[type] = wrap;
      }

      return addListener.apply(this, arguments);
    };

    process.removeListener = function(type, listener) {
      var ret = removeListener.apply(this, arguments);
      if (isSignal(type)) {
        assert(signalWraps.hasOwnProperty(type));

        if (this.listeners(type).length === 0) {
          signalWraps[type].close();
          delete signalWraps[type];
        }
      }

      return ret;
    };
  };


  startup.processChannel = function() {
    // If we were spawned with env NODE_CHANNEL_FD then load that up and
    // start parsing data from that stream.
    if (process.env.NODE_CHANNEL_FD) {
      var fd = parseInt(process.env.NODE_CHANNEL_FD, 10);
      assert(fd >= 0);

      // Make sure it's not accidentally inherited by child processes.
      delete process.env.NODE_CHANNEL_FD;

      var cp = NativeModule.require('child_process');

      // Load tcp_wrap to avoid situation where we might immediately receive
      // a message.
      // FIXME is this really necessary?
      process.binding('tcp_wrap');

      cp._forkChild(fd);
      assert(process.send);
    }
  };


  startup.processRawDebug = function() {
    var format = NativeModule.require('util').format;
    var rawDebug = process._rawDebug;
    process._rawDebug = function() {
      rawDebug(format.apply(null, arguments));
    };
  };


  startup.resolveArgv0 = function() {
    var cwd = process.cwd();
    var isWindows = process.platform === 'win32';

    // Make process.argv[0] into a full path, but only touch argv[0] if it's
    // not a system $PATH lookup.
    // TODO: Make this work on Windows as well.  Note that "node" might
    // execute cwd\node.exe, or some %PATH%\node.exe on Windows,
    // and that every directory has its own cwd, so d:node.exe is valid.
    var argv0 = process.argv[0];
    if (!isWindows && argv0.indexOf('/') !== -1 && argv0.charAt(0) !== '/') {
      var path = NativeModule.require('path');
      process.argv[0] = path.join(cwd, process.argv[0]);
    }
  };

  // Below you find a minimal module system, which is used to load the node
  // core modules found in lib/*.js. All core modules are compiled into the
  // node binary, so they can be loaded faster.

  var ContextifyScript = process.binding('contextify').ContextifyScript;
  function runInThisContext(code, options) {
    var script = new ContextifyScript(code, options);
    return script.runInThisContext();
  }

  function NativeModule(id) {
    this.filename = id + '.js';
    this.id = id;
    this.exports = {};
    this.loaded = false;
  }

  NativeModule._source = process.binding('natives');
  NativeModule._cache = {};

  NativeModule.require = function(id) {
    if (id == 'native_module') {
      return NativeModule;
    }

    var cached = NativeModule.getCached(id);
    if (cached) {
      return cached.exports;
    }

    if (!NativeModule.exists(id)) {
      throw new Error('No such native module ' + id);
    }

    process.moduleLoadList.push('NativeModule ' + id);

    var nativeModule = new NativeModule(id);

    nativeModule.cache();
    nativeModule.compile();

    return nativeModule.exports;
  };

  NativeModule.getCached = function(id) {
    return NativeModule._cache[id];
  }

  NativeModule.exists = function(id) {
    return NativeModule._source.hasOwnProperty(id);
  }

  NativeModule.getSource = function(id) {
    return NativeModule._source[id];
  }

  NativeModule.wrap = function(script) {
    return NativeModule.wrapper[0] + script + NativeModule.wrapper[1];
  };

  NativeModule.wrapper = [
    '(function (exports, require, module, __filename, __dirname) { ',
    '\n});'
  ];

  NativeModule.prototype.compile = function() {
    var source = NativeModule.getSource(this.id);
    source = NativeModule.wrap(source);

    var fn = runInThisContext(source, { filename: this.filename });
    fn(this.exports, NativeModule.require, this, this.filename);

    this.loaded = true;
  };

  NativeModule.prototype.cache = function() {
    NativeModule._cache[this.id] = this;
  };

  startup();
});
