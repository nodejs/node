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

    startup.processAssert();
    startup.processConfig();
    startup.processNextTick();
    startup.processStdio();
    startup.processKillAndExit();
    startup.processSignalHandlers();

    startup.processChannel();

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

      // If this is a worker in cluster mode, start up the communiction
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
    process.binding('buffer').setFastBufferConstructor(global.Buffer);
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
    // call into the active domain, or emit uncaughtException,
    // and exit if there are no listeners.
    process._fatalException = function(er) {
      var caught = false;
      if (process.domain) {
        var domain = process.domain;
        var domainModule = NativeModule.require('domain');
        var domainStack = domainModule._stack;

        // ignore errors on disposed domains.
        //
        // XXX This is a bit stupid.  We should probably get rid of
        // domain.dispose() altogether.  It's almost always a terrible
        // idea.  --isaacs
        if (domain._disposed)
          return true;

        er.domain = domain;
        er.domainThrown = true;
        // wrap this in a try/catch so we don't get infinite throwing
        try {
          // One of three things will happen here.
          //
          // 1. There is a handler, caught = true
          // 2. There is no handler, caught = false
          // 3. It throws, caught = false
          //
          // If caught is false after this, then there's no need to exit()
          // the domain, because we're going to crash the process anyway.
          caught = domain.emit('error', er);

          // Exit all domains on the stack.  Uncaught exceptions end the
          // current tick and no domains should be left on the stack
          // between ticks.
          var domainModule = NativeModule.require('domain');
          domainStack.length = 0;
          domainModule.active = process.domain = null;
        } catch (er2) {
          // The domain error handler threw!  oh no!
          // See if another domain can catch THIS error,
          // or else crash on the original one.
          // If the user already exited it, then don't double-exit.
          if (domain === domainModule.active)
            domainStack.pop();
          if (domainStack.length) {
            var parentDomain = domainStack[domainStack.length - 1];
            process.domain = domainModule.active = parentDomain;
            caught = process._fatalException(er2);
          } else
            caught = false;
        }
      } else {
        caught = process.emit('uncaughtException', er);
      }
      // if someone handled it, then great.  otherwise, die in C++ land
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
      }
      // if we handled an error, then make sure any ticks get processed
      if (caught)
        process._needTickCallback();
      return caught;
    };
  };

  var assert;
  startup.processAssert = function() {
    // Note that calls to assert() are pre-processed out by JS2C for the
    // normal build of node. They persist only in the node_g build.
    // Similarly for debug().
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
    var _needTickCallback = process._needTickCallback;
    var nextTickQueue = [];
    var needSpinner = true;
    var inTick = false;

    // this infobox thing is used so that the C++ code in src/node.cc
    // can have easy accesss to our nextTick state, and avoid unnecessary
    // calls into process._tickCallback.
    // order is [length, index, depth]
    // Never write code like this without very good reason!
    var infoBox = process._tickInfoBox;
    var length = 0;
    var index = 1;
    var depth = 2;

    process.nextTick = nextTick;
    // needs to be accessible from cc land
    process._nextDomainTick = _nextDomainTick;
    process._tickCallback = _tickCallback;
    process._tickDomainCallback = _tickDomainCallback;
    process._tickFromSpinner = _tickFromSpinner;

    // the maximum number of times it'll process something like
    // nextTick(function f(){nextTick(f)})
    // It's unlikely, but not illegal, to hit this limit.  When
    // that happens, it yields to libuv's tick spinner.
    // This is a loop counter, not a stack depth, so we aren't using
    // up lots of memory here.  I/O can sneak in before nextTick if this
    // limit is hit, which is not ideal, but not terrible.
    process.maxTickDepth = 1000;

    function tickDone(tickDepth_) {
      if (infoBox[length] !== 0) {
        if (infoBox[length] <= infoBox[index]) {
          nextTickQueue = [];
          infoBox[length] = 0;
        } else {
          nextTickQueue.splice(0, infoBox[index]);
          infoBox[length] = nextTickQueue.length;
          if (needSpinner) {
            _needTickCallback();
            needSpinner = false;
          }
        }
      }
      inTick = false;
      infoBox[index] = 0;
      infoBox[depth] = tickDepth_;
    }

    function maxTickWarn() {
      // XXX Remove all this maxTickDepth stuff in 0.11
      var msg = '(node) warning: Recursive process.nextTick detected. ' +
                'This will break in the next version of node. ' +
                'Please use setImmediate for recursive deferral.';
      if (process.throwDeprecation)
        throw new Error(msg);
      else if (process.traceDeprecation)
        console.trace(msg);
      else
        console.error(msg);
    }

    function _tickFromSpinner() {
      needSpinner = true;
      // coming from spinner, reset!
      if (infoBox[depth] !== 0)
        infoBox[depth] = 0;
      // no callbacks to run
      if (infoBox[length] === 0)
        return infoBox[index] = infoBox[depth] = 0;
      process._tickCallback();
    }

    // run callbacks that have no domain
    // using domains will cause this to be overridden
    function _tickCallback() {
      var callback, nextTickLength, threw;

      if (inTick) return;
      if (infoBox[length] === 0) {
        infoBox[index] = 0;
        infoBox[depth] = 0;
        return;
      }
      inTick = true;

      while (infoBox[depth]++ < process.maxTickDepth) {
        nextTickLength = infoBox[length];
        if (infoBox[index] === nextTickLength)
          return tickDone(0);

        while (infoBox[index] < nextTickLength) {
          callback = nextTickQueue[infoBox[index]++].callback;
          threw = true;
          try {
            callback();
            threw = false;
          } finally {
            if (threw) tickDone(infoBox[depth]);
          }
        }
      }

      tickDone(0);
    }

    function _tickDomainCallback() {
      var nextTickLength, tock, callback, threw;

      // if you add a nextTick in a domain's error handler, then
      // it's possible to cycle indefinitely.  Normally, the tickDone
      // in the finally{} block below will prevent this, however if
      // that error handler ALSO triggers multiple MakeCallbacks, then
      // it'll try to keep clearing the queue, since the finally block
      // fires *before* the error hits the top level and is handled.
      if (infoBox[depth] >= process.maxTickDepth)
        return _needTickCallback();

      if (inTick) return;
      inTick = true;

      // always do this at least once.  otherwise if process.maxTickDepth
      // is set to some negative value, or if there were repeated errors
      // preventing depth from being cleared, we'd never process any
      // of them.
      while (infoBox[depth]++ < process.maxTickDepth) {
        nextTickLength = infoBox[length];
        if (infoBox[index] === nextTickLength)
          return tickDone(0);

        while (infoBox[index] < nextTickLength) {
          tock = nextTickQueue[infoBox[index]++];
          callback = tock.callback;
          if (tock.domain) {
            if (tock.domain._disposed) continue;
            tock.domain.enter();
          }
          threw = true;
          try {
            callback();
            threw = false;
          } finally {
            // finally blocks fire before the error hits the top level,
            // so we can't clear the depth at this point.
            if (threw) tickDone(infoBox[depth]);
          }
          if (tock.domain) {
            tock.domain.exit();
          }
        }
      }

      tickDone(0);
    }

    function nextTick(callback) {
      // on the way out, don't bother. it won't get fired anyway.
      if (process._exiting)
        return;
      if (infoBox[depth] >= process.maxTickDepth)
        maxTickWarn();

      var obj = { callback: callback, domain: null };

      nextTickQueue.push(obj);
      infoBox[length]++;

      if (needSpinner) {
        _needTickCallback();
        needSpinner = false;
      }
    }

    function _nextDomainTick(callback) {
      // on the way out, don't bother. it won't get fired anyway.
      if (process._exiting)
        return;
      if (infoBox[depth] >= process.maxTickDepth)
        maxTickWarn();

      var obj = { callback: callback, domain: process.domain };

      nextTickQueue.push(obj);
      infoBox[length]++;

      if (needSpinner) {
        _needTickCallback();
        needSpinner = false;
      }
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
               JSON.stringify(body) + ', ' +
               JSON.stringify(name) + ', true);\n';
    }
    var result = module._compile(script, name + '-wrapper');
    if (process._print_eval) console.log(result);
  }

  function errnoException(errorno, syscall) {
    // TODO make this more compatible with ErrnoException from src/node.cc
    // Once all of Node is using this function the ErrnoException from
    // src/node.cc should be removed.
    var e = new Error(syscall + ' ' + errorno);
    e.errno = e.code = errorno;
    e.syscall = syscall;
    return e;
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
    process.exit = function(code) {
      if (!process._exiting) {
        process._exiting = true;
        process.emit('exit', code || 0);
      }
      process.reallyExit(code || 0);
    };

    process.kill = function(pid, sig) {
      var r;

      // preserve null signal
      if (0 === sig) {
        r = process._kill(pid, 0);
      } else {
        sig = sig || 'SIGTERM';
        if (startup.lazyConstants()[sig]) {
          r = process._kill(pid, startup.lazyConstants()[sig]);
        } else {
          throw new Error('Unknown signal: ' + sig);
        }
      }

      if (r) {
        throw errnoException(process._errno, 'kill');
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
        var r = wrap.start(signum);
        if (r) {
          wrap.close();
          throw errnoException(process._errno, 'uv_signal_start');
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
  }

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

  var Script = process.binding('evals').NodeScript;
  var runInThisContext = Script.runInThisContext;

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

    var fn = runInThisContext(source, this.filename, true);
    fn(this.exports, NativeModule.require, this, this.filename);

    this.loaded = true;
  };

  NativeModule.prototype.cache = function() {
    NativeModule._cache[this.id] = this;
  };

  startup();
});
