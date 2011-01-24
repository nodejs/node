(function(process) {

  global = this;
  global.process = process;
  global.global = global;
  global.GLOBAL = global;
  global.root = global;

  /** deprecation errors ************************************************/

  function removed(reason) {
    return function() {
      throw new Error(reason);
    };
  }

  process.debug =
      removed('process.debug() use console.error() instead');
  process.error =
      removed('process.error() use console.error() instead');
  process.watchFile =
      removed('process.watchFile() has moved to fs.watchFile()');
  process.unwatchFile =
      removed('process.unwatchFile() has moved to fs.unwatchFile()');
  process.mixin =
      removed('process.mixin() has been removed.');
  process.createChildProcess =
      removed('childProcess API has changed. See doc/api.txt.');
  process.inherits =
      removed('process.inherits() has moved to sys.inherits.');
  process._byteLength =
      removed('process._byteLength() has moved to Buffer.byteLength');

  process.assert = function(x, msg) {
    if (!x) throw new Error(msg || 'assertion error');
  };

  var Script = process.binding('evals').Script;
  var runInThisContext = Script.runInThisContext;

  // lazy loaded.
  var constants;
  function lazyConstants() {
    if (!constants) constants = process.binding('constants');
    return constants;
  }


  // nextTick()

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

  // Native modules don't need a full require function. So we can bootstrap
  // most of the system with this mini module system.
  var NativeModule = (function() {
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

    return NativeModule;
  })();

  var Module = NativeModule.require('module').Module;

  // Load events module in order to access prototype elements on process like
  // process.addListener.
  var events = NativeModule.require('events');

  // Signal Handlers
  (function() {
    var signalWatchers = {};
    var addListener = process.addListener;
    var removeListener = process.removeListener;

    function isSignal(event) {
      return event.slice(0, 3) === 'SIG' && lazyConstants()[event];
    }

    // Wrap addListener for the special signal types
    process.on = process.addListener = function(type, listener) {
      var ret = addListener.apply(this, arguments);
      if (isSignal(type)) {
        if (!signalWatchers.hasOwnProperty(type)) {
          var b = process.binding('signal_watcher');
          var w = new b.SignalWatcher(lazyConstants()[type]);
          w.callback = function() { process.emit(type); };
          signalWatchers[type] = w;
          w.start();

        } else if (this.listeners(type).length === 1) {
          signalWatchers[event].start();
        }
      }

      return ret;
    };

    process.removeListener = function(type, listener) {
      var ret = removeListener.apply(this, arguments);
      if (isSignal(type)) {
        process.assert(signalWatchers.hasOwnProperty(type));

        if (this.listeners(type).length === 0) {
          signalWatchers[type].stop();
        }
      }

      return ret;
    };
  })();


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


  var stdout, stdin;


  process.__defineGetter__('stdout', function() {
    if (stdout) return stdout;

    var binding = process.binding('stdio'),
        net = NativeModule.require('net'),
        fs = NativeModule.require('fs'),
        tty = NativeModule.require('tty'),
        fd = binding.stdoutFD;

    if (binding.isatty(fd)) {
      stdout = new tty.WriteStream(fd);
    } else if (binding.isStdoutBlocking()) {
      stdout = new fs.WriteStream(null, {fd: fd});
    } else {
      stdout = new net.Stream(fd);
      // FIXME Should probably have an option in net.Stream to create a
      // stream from an existing fd which is writable only. But for now
      // we'll just add this hack and set the `readable` member to false.
      // Test: ./node test/fixtures/echo.js < /etc/passwd
      stdout.readable = false;
    }

    return stdout;
  });


  process.__defineGetter__('stdin', function() {
    if (stdin) return stdin;

    var binding = process.binding('stdio'),
        net = NativeModule.require('net'),
        fs = NativeModule.require('fs'),
        tty = NativeModule.require('tty'),
        fd = binding.openStdin();

    if (binding.isatty(fd)) {
      stdin = new tty.ReadStream(fd);
    } else if (binding.isStdinBlocking()) {
      stdin = new fs.ReadStream(null, {fd: fd});
    } else {
      stdin = new net.Stream(fd);
      stdin.readable = true;
    }

    return stdin;
  });


  process.openStdin = function() {
    process.stdin.resume();
    return process.stdin;
  };


  // Lazy load console object
  global.__defineGetter__('console', function() {
    return NativeModule.require('console');
  });


  global.Buffer = NativeModule.require('buffer').Buffer;

  process.exit = function(code) {
    process.emit('exit', code || 0);
    process.reallyExit(code || 0);
  };

  process.kill = function(pid, sig) {
    sig = sig || 'SIGTERM';
    if (!lazyConstants()[sig]) throw new Error('Unknown signal: ' + sig);
    process._kill(pid, lazyConstants()[sig]);
  };


  var cwd = process.cwd();
  var path = NativeModule.require('path');
  var isWindows = process.platform === 'win32';

  // Make process.argv[0] and process.argv[1] into full paths, but only
  // touch argv[0] if it's not a system $PATH lookup.
  // TODO: Make this work on Windows as well.  Note that "node" might
  // execute cwd\node.exe, or some %PATH%\node.exe on Windows,
  // and that every directory has its own cwd, so d:node.exe is valid.
  var argv0 = process.argv[0];
  if (!isWindows && argv0.indexOf('/') !== -1 && argv0.charAt(0) !== '/') {
    process.argv[0] = path.join(cwd, process.argv[0]);
  }

  // To allow people to extend Node in different ways, this hook allows
  // one to drop a file lib/_third_party_main.js into the build directory
  // which will be executed instead of Node's normal loading.
  if (NativeModule.exists('_third_party_main')) {
    process.nextTick(function() {
      NativeModule.require('_third_party_main');
    });
    return;
  }

  if (process.argv[1]) {
    if (process.argv[1] == 'debug') {
      // Start the debugger agent
      var d = NativeModule.require('_debugger');
      d.start();
      return;
    }

    // Load Module
    // make process.argv[1] into a full path
    if (!(/^http:\/\//).exec(process.argv[1])) {
      process.argv[1] = path.resolve(process.argv[1]);
    }
    // REMOVEME: nextTick should not be necessary. This hack to get
    // test/simple/test-exception-handler2.js working.
    process.nextTick(Module.runMain);
    return;
  }

  if (process._eval) {
    // -e, --eval
    var rv = new Module()._compile('return eval(process._eval)', 'eval');
    console.log(rv);
    return;
  }

  // REPL
  Module.requireRepl().start();
});
