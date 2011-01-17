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
  var runInNewContext = Script.runInNewContext;

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
      if (id == 'module') {
        return Module;
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

  // Module System
  var Module = (function() {
    function Module(id, parent) {
      this.id = id;
      this.exports = {};
      this.parent = parent;

      this.filename = null;
      this.loaded = false;
      this.exited = false;
      this.children = [];
    };

    // Set the environ variable NODE_MODULE_CONTEXTS=1 to make node load all
    // modules in thier own context.
    Module._contextLoad = (+process.env['NODE_MODULE_CONTEXTS'] > 0);
    Module._cache = {};
    Module._pathCache = {};
    Module._extensions = {};
    Module._paths = [];

    Module.wrapper = NativeModule.wrapper;
    Module.wrap = NativeModule.wrap;

    var path = NativeModule.require('path');

    Module._debug = function() {};
    if (process.env.NODE_DEBUG && /module/.test(process.env.NODE_DEBUG)) {
      Module._debug = function(x) {
        console.error(x);
      };
    }

    // We use this alias for the preprocessor that filters it out
    var debug = Module._debug;

    // given a module name, and a list of paths to test, returns the first
    // matching file in the following precedence.
    //
    // require("a.<ext>")
    //   -> a.<ext>
    //
    // require("a")
    //   -> a
    //   -> a.<ext>
    //   -> a/index.<ext>
    Module._findPath = function(request, paths) {
      var fs = NativeModule.require('fs');
      var exts = Object.keys(Module._extensions);

      if (request.charAt(0) === '/') {
        paths = [''];
      }

      // check if the file exists and is not a directory
      function tryFile(requestPath) {
        try {
          var stats = fs.statSync(requestPath);
          if (stats && !stats.isDirectory()) {
            return fs.realpathSync(requestPath);
          }
        } catch (e) {}
        return false;
      };

      // given a path check a the file exists with any of the set extensions
      function tryExtensions(p, extension) {
        for (var i = 0, EL = exts.length; i < EL; i++) {
          var filename = tryFile(p + exts[i]);

          if (filename) {
            return filename;
          }
        }
        return false;
      };

      var cacheKey = JSON.stringify({request: request, paths: paths});
      if (Module._pathCache[cacheKey]) {
        return Module._pathCache[cacheKey];
      }

      // For each path
      for (var i = 0, PL = paths.length; i < PL; i++) {
        var basePath = path.resolve(paths[i], request);

        // try to join the request to the path
        var filename = tryFile(basePath);

        if (!filename) {
          // try it with each of the extensions
          filename = tryExtensions(basePath)
        }

        if (!filename) {
          // try it with each of the extensions at "index"
          filename = tryExtensions(path.resolve(basePath, 'index'))
        }

        if (filename) {
          Module._pathCache[cacheKey] = filename;
          return filename;
        }
      }
      return false;
    }

    Module._resolveLookupPaths = function(request, parent) {
      if (NativeModule.exists(request)) {
        return [request, []];
      }

      var start = request.substring(0, 2);
      if (start !== './' && start !== '..') {
        return [request, Module._paths];
      }

      // with --eval, parent.id is not set and parent.filename is null
      if (!parent || !parent.id || !parent.filename) {
        // make require('./path/to/foo') work - normally the path is taken
        // from realpath(__filename) but with eval there is no filename
        return [request, ['.'].concat(Module._paths)];
      }

      // Is the parent an index module?
      // We can assume the parent has a valid extension,
      // as it already has been accepted as a module.
      var isIndex = /^index\.\w+?$/.test(path.basename(parent.filename));
      var parentIdPath = isIndex ? parent.id : path.dirname(parent.id);
      var id = path.resolve(parentIdPath, request);

      // make sure require('./path') and require('path') get distinct ids, even
      // when called from the toplevel js file
      if (parentIdPath === '.' && id.indexOf('/') === -1) {
        id = './' + id;
      }

      debug('RELATIVE: requested:' + request +
            ' set ID to: ' + id + ' from ' + parent.id);

      return [id, [path.dirname(parent.filename)]];
    }


    Module._load = function(request, parent) {
      debug('Module._load REQUEST  ' + (request) +
            ' parent: ' + parent.id);

      var resolved = Module._resolveFilename(request, parent);
      var id = resolved[0];
      var filename = resolved[1];

      var cachedModule = Module._cache[filename];
      if (cachedModule) {
        return cachedModule.exports;
      }

      if (NativeModule.exists(id)) {
        // REPL is a special case, because it needs the real require.
        if (id == 'repl') {
          var replModule = new Module('repl');
          replModule._compile(NativeModule.getSource('repl'), 'repl.js');
          NativeModule._cache.repl = replModule;
          return replModule.exports;
        }

        debug('load native module ' + request);
        return NativeModule.require(id);
      }

      var module = new Module(id, parent);
      Module._cache[filename] = module;
      module.load(filename);
      return module.exports;
    };

    Module._resolveFilename = function(request, parent) {
      if (NativeModule.exists(request)) {
        return [request, request];
      }

      var resolvedModule = Module._resolveLookupPaths(request, parent);
      var id = resolvedModule[0];
      var paths = resolvedModule[1];

      // look up the filename first, since that's the cache key.
      debug('looking for ' + JSON.stringify(id) +
            ' in ' + JSON.stringify(paths));

      var filename = Module._findPath(request, paths);
      if (!filename) {
        throw new Error("Cannot find module '" + request + "'");
      }
      id = filename;
      return [id, filename];
    }


    Module.prototype.load = function(filename) {
      debug('load ' + JSON.stringify(filename) +
            ' for module ' + JSON.stringify(this.id));

      process.assert(!this.loaded);
      this.filename = filename;

      var extension = path.extname(filename) || '.js';
      if (!Module._extensions[extension]) extension = '.js';
      Module._extensions[extension](this, filename);
      this.loaded = true;
    };


    // Returns exception if any
    Module.prototype._compile = function(content, filename) {
      var self = this;
      // remove shebang
      content = content.replace(/^\#\!.*/, '');

      function require(path) {
        return Module._load(path, self);
      }

      require.resolve = function(request) {
        return Module._resolveFilename(request, self)[1];
      }
      require.paths = Module._paths;
      require.main = process.mainModule;
      // Enable support to add extra extension types
      require.extensions = Module._extensions;
      require.registerExtension = removed('require.registerExtension() ' +
                                          'removed. Use require.extensions ' +
                                          'instead.');

      require.cache = Module._cache;

      var dirname = path.dirname(filename);

      if (Module._contextLoad) {
        if (self.id !== '.') {
          debug('load submodule');
          // not root module
          var sandbox = {};
          for (var k in global) {
            sandbox[k] = global[k];
          }
          sandbox.require = require;
          sandbox.exports = self.exports;
          sandbox.__filename = filename;
          sandbox.__dirname = dirname;
          sandbox.module = self;
          sandbox.global = sandbox;
          sandbox.root = root;

          return runInNewContext(content, sandbox, filename, true);
        }

        debug('load root module');
        // root module
        global.require = require;
        global.exports = self.exports;
        global.__filename = filename;
        global.__dirname = dirname;
        global.module = self;

        return runInThisContext(content, filename, true);
      }

      // create wrapper function
      var wrapper = Module.wrap(content);

      var compiledWrapper = runInThisContext(wrapper, filename, true);
      if (filename === process.argv[1] && global.v8debug) {
        global.v8debug.Debug.setBreakPoint(compiledWrapper, 0, 0);
      }
      var args = [self.exports, require, self, filename, dirname];
      return compiledWrapper.apply(self.exports, args);
    };

    // Native extension for .js
    Module._extensions['.js'] = function(module, filename) {
      var content = NativeModule.require('fs').readFileSync(filename, 'utf8');
      module._compile(content, filename);
    };


    // Native extension for .node
    Module._extensions['.node'] = function(module, filename) {
      process.dlopen(filename, module.exports);
    };


    // bootstrap main module.
    Module.runMain = function() {
      // Load the main module--the command line argument.
      process.mainModule = new Module('.');
      process.mainModule.load(process.argv[1]);
    };

    Module._initPaths = function() {
      var paths = [path.resolve(process.execPath, '..', '..', 'lib', 'node')];

      if (process.env['HOME']) {
        paths.unshift(path.resolve(process.env['HOME'], '.node_libraries'));
        paths.unshift(path.resolve(process.env['HOME'], '.node_modules'));
      }

      if (process.env['NODE_PATH']) {
        paths = process.env['NODE_PATH'].split(':').concat(paths);
      }

      Module._paths = paths;
    };

    // bootstrap repl
    Module.requireRepl = function() {
      return Module._load('repl', '.');
    };

    Module._initPaths();

    // backwards compatibility
    Module.Module = Module;

    return Module;
  })();


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
    process.nextTick(function () {
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
