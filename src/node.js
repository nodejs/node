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
  global = this;

  function startup() {
    startup.globalVariables();
    startup.globalTimeouts();
    startup.globalConsole();

    startup.processAssert();
    startup.processNextTick();
    startup.processStdio();
    startup.processKillAndExit();
    startup.processSignalHandlers();

    startup.processChannel();

    startup.removedMethods();

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

    } else if (process.argv[1]) {
      // make process.argv[1] into a full path
      if (!(/^http:\/\//).exec(process.argv[1])) {
        var path = NativeModule.require('path');
        process.argv[1] = path.resolve(process.argv[1]);
      }

      var Module = NativeModule.require('module');
      // REMOVEME: nextTick should not be necessary. This hack to get
      // test/simple/test-exception-handler2.js working.
      // Main entry point into most programs:
      process.nextTick(Module.runMain);

    } else if (process._eval != null) {
      // User passed '-e' or '--eval' arguments to Node.
      var Module = NativeModule.require('module');
      var rv = new Module()._compile('return eval(process._eval)', 'eval');
      console.log(rv);

    } else {
      var binding = process.binding('stdio');
      var fd = binding.openStdin();
      var Module = NativeModule.require('module');

      if (NativeModule.require('tty').isatty(fd)) {
        // REPL
        Module.requireRepl().start();

      } else {
        // Read all of stdin - execute it.
        process.stdin.resume();
        process.stdin.setEncoding('utf8');

        var code = '';
        process.stdin.on('data', function(d) {
          code += d;
        });

        process.stdin.on('end', function() {
          new Module()._compile(code, '[stdin]');
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
    if (process.cov) {
      global.__cov = {};
    }
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
  };

  startup.globalConsole = function() {
    global.console = NativeModule.require('console');
  };

  startup._lazyConstants = null;

  startup.lazyConstants = function() {
    if (!startup._lazyConstants) {
      startup._lazyConstants = process.binding('constants');
    }
    return startup._lazyConstants;
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

  startup.processNextTick = function() {
    var nextTickQueue = [];

    process._tickCallback = function() {
      var l = nextTickQueue.length;
      if (l === 0) return;

      try {
        for (var i = 0; i < l; i++) {
          nextTickQueue[i]();
        }
      }
      catch (e) {
        nextTickQueue.splice(0, i + 1);
        if (i + 1 < l) {
          process._needTickCallback();
        }
        throw e; // process.nextTick error, or 'error' event on first tick
      }

      nextTickQueue.splice(0, l);
    };

    process.nextTick = function(callback) {
      nextTickQueue.push(callback);
      process._needTickCallback();
    };
  };

  startup.processStdio = function() {
    var binding = process.binding('stdio'),
        // FIXME Remove conditional when net is supported again on windows.
        net = (process.platform !== "win32")
              ? NativeModule.require('net')
              : undefined,
        fs = NativeModule.require('fs'),
        tty = NativeModule.require('tty');

    // process.stdout

    var fd = binding.stdoutFD;

    if (binding.isatty(fd)) {
      process.stdout = new tty.WriteStream(fd);
    } else if (binding.isStdoutBlocking()) {
      process.stdout = new fs.WriteStream(null, {fd: fd});
    } else {
      process.stdout = new net.Stream(fd);
      // FIXME Should probably have an option in net.Stream to create a
      // stream from an existing fd which is writable only. But for now
      // we'll just add this hack and set the `readable` member to false.
      // Test: ./node test/fixtures/echo.js < /etc/passwd
      process.stdout.readable = false;
    }

    // process.stderr

    var events = NativeModule.require('events');
    var stderr = process.stderr = new events.EventEmitter();
    stderr.writable = true;
    stderr.readable = false;
    stderr.write = process.binding('stdio').writeError;
    stderr.end = stderr.destroy = stderr.destroySoon = function() { };

    // process.stdin

    var fd = binding.openStdin();

    if (binding.isatty(fd)) {
      process.stdin = new tty.ReadStream(fd);
    } else if (binding.isStdinBlocking()) {
      process.stdin = new fs.ReadStream(null, {fd: fd});
    } else {
      process.stdin = new net.Stream(fd);
      process.stdin.readable = true;
    }

    process.openStdin = function() {
      process.stdin.resume();
      return process.stdin;
    };
  };

  startup.processKillAndExit = function() {
    process.exit = function(code) {
      process.emit('exit', code || 0);
      process.reallyExit(code || 0);
    };

    process.kill = function(pid, sig) {
      // preserve null signal
      if (0 === sig) {
        process._kill(pid, 0);
      } else {
        sig = sig || 'SIGTERM';
        if (startup.lazyConstants()[sig]) {
          process._kill(pid, startup.lazyConstants()[sig]);
        } else {
          throw new Error('Unknown signal: ' + sig);
        }
      }
    };
  };

  startup.processSignalHandlers = function() {
    // Load events module in order to access prototype elements on process like
    // process.addListener.
    var events = NativeModule.require('events');
    var signalWatchers = {};
    var addListener = process.addListener;
    var removeListener = process.removeListener;

    function isSignal(event) {
      return event.slice(0, 3) === 'SIG' && startup.lazyConstants()[event];
    }

    // Wrap addListener for the special signal types
    process.on = process.addListener = function(type, listener) {
      var ret = addListener.apply(this, arguments);
      if (isSignal(type)) {
        if (!signalWatchers.hasOwnProperty(type)) {
          var b = process.binding('signal_watcher');
          var w = new b.SignalWatcher(startup.lazyConstants()[type]);
          w.callback = function() { process.emit(type); };
          signalWatchers[type] = w;
          w.start();

        } else if (this.listeners(type).length === 1) {
          signalWatchers[type].start();
        }
      }

      return ret;
    };

    process.removeListener = function(type, listener) {
      var ret = removeListener.apply(this, arguments);
      if (isSignal(type)) {
        assert(signalWatchers.hasOwnProperty(type));

        if (this.listeners(type).length === 0) {
          signalWatchers[type].stop();
        }
      }

      return ret;
    };
  };


  startup.processChannel = function() {
    // If we were spawned with env NODE_CHANNEL_FD then load that up and
    // start parsing data from that stream.
    if (process.env.NODE_CHANNEL_FD) {
      var fd = parseInt(process.env.NODE_CHANNEL_FD);
      assert(fd >= 0);
      var cp = NativeModule.require('child_process');
      cp._forkChild(fd);
      assert(process.send);
    }
  }

  startup._removedProcessMethods = {
    'assert': 'process.assert() use require("assert").ok() instead',
    'debug': 'process.debug() use console.error() instead',
    'error': 'process.error() use console.error() instead',
    'watchFile': 'process.watchFile() has moved to fs.watchFile()',
    'unwatchFile': 'process.unwatchFile() has moved to fs.unwatchFile()',
    'mixin': 'process.mixin() has been removed.',
    'createChildProcess': 'childProcess API has changed. See doc/api.txt.',
    'inherits': 'process.inherits() has moved to sys.inherits.',
    '_byteLength': 'process._byteLength() has moved to Buffer.byteLength',
  };

  startup.removedMethods = function() {
    for (var method in startup._removedProcessMethods) {
      var reason = startup._removedProcessMethods[method];
      process[method] = startup._removedMethod(reason);
    }
  };

  startup._removedMethod = function(reason) {
    return function() {
      throw new Error(reason);
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

    if (process.cov) {
      process.on('exit', function() {
        var coverage = JSON.stringify(__cov);
        var path = NativeModule.require('path');
        var fs = NativeModule.require('fs');
        var filename = path.join(cwd, 'node-cov.json');
        try {
          fs.unlinkSync(filename);
        } catch(e) {
        }
        fs.writeFileSync(filename, coverage);
      });
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

    var nativeModule = new NativeModule(id);

    nativeModule.compile();
    nativeModule.cache();

    return nativeModule.exports;
  };

  NativeModule.getCached = function(id) {
    return NativeModule._cache[id];
  }

  NativeModule.exists = function(id) {
    return (id in NativeModule._source);
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
