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

  var evals = process.binding('evals');

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

  var internalModuleCache = {};

  // This contains the source code for the files in lib/
  // Like, natives.fs is the contents of lib/fs.js
  var natives = process.binding('natives');

  // Native modules don't need a full require function. So we can bootstrap
  // most of the system with this mini-require.
  function requireNative(id) {
    if (internalModuleCache[id]) return internalModuleCache[id].exports;
    if (!natives[id]) throw new Error('No such native module ' + id);

    var fn = evals.Script.runInThisContext(
        '(function (module, exports, require) {' + natives[id] + '\n})',
        id + '.js');
    var m = {id: id, exports: {}};
    fn(m, m.exports, requireNative);
    m.loaded = true;
    internalModuleCache[id] = m;
    return m.exports;
  }

  // Module System
  var module = (function() {
    var exports = {};
    // Set the environ variable NODE_MODULE_CONTEXTS=1 to make node load all
    // modules in thier own context.
    var contextLoad = false;
    if (+process.env['NODE_MODULE_CONTEXTS'] > 0) contextLoad = true;

    var moduleCache = {};

    function Module(id, parent) {
      this.id = id;
      this.exports = {};
      this.parent = parent;

      this.filename = null;
      this.loaded = false;
      this.exited = false;
      this.children = [];
    };


    // Modules

    var debugLevel = parseInt(process.env['NODE_DEBUG'], 16);
    var debug;
    if (debugLevel & 1) {
      debug = function(x) { console.error(x); };
    } else {
      debug = function() { };
    }

    var path = requireNative('path');

    var modulePaths = [path.join(process.execPath, '..', '..', 'lib', 'node')];

    if (process.env['HOME']) {
      modulePaths.unshift(path.join(process.env['HOME'], '.node_libraries'));
      modulePaths.unshift(path.join(process.env['HOME'], '.node_modules'));
    }

    if (process.env['NODE_PATH']) {
      modulePaths = process.env['NODE_PATH'].split(':').concat(modulePaths);
    }

    var extensions = {};
    var registerExtension =
        removed('require.registerExtension() removed.' +
                ' Use require.extensions instead');

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
    function findModulePath(request, paths) {
      var fs = requireNative('fs'),
          exts = Object.keys(extensions);

      paths = request.charAt(0) === '/' ? [''] : paths;

      // check if the file exists and is not a directory
      var tryFile = function(requestPath) {
        try {
          var stats = fs.statSync(requestPath);
          if (stats && !stats.isDirectory()) {
            return requestPath;
          }
        } catch (e) {}
        return false;
      };

      // given a path check a the file exists with any of the set extensions
      var tryExtensions = function(p, extension) {
        for (var i = 0, EL = exts.length; i < EL; i++) {
          f = tryFile(p + exts[i]);
          if (f) { return f; }
        }
        return false;
      };

      // For each path
      for (var i = 0, PL = paths.length; i < PL; i++) {
        var p = paths[i],
            // try to join the request to the path
            f = tryFile(path.join(p, request)) ||
            // try it with each of the extensions
            tryExtensions(path.join(p, request)) ||
            // try it with each of the extensions at "index"
            tryExtensions(path.join(p, request, 'index'));
        if (f) { return f; }
      }
      return false;
    }

    // sync - no i/o performed
    function resolveModuleLookupPaths(request, parent) {

      if (natives[request]) return [request, []];

      var start = request.substring(0, 2);
      if (start !== './' && start !== '..') {
        return [request, modulePaths];
      }

      // with --eval, parent.id is not set and parent.filename is null
      if (!parent || !parent.id || !parent.filename) {
        // make require('./path/to/foo') work - normally the path is taken
        // from realpath(__filename) but with eval there is no filename
        return [request, ['.'].concat(modulePaths)];
      }

      // Is the parent an index module?
      // We can assume the parent has a valid extension,
      // as it already has been accepted as a module.
      var isIndex = /^index\.\w+?$/.test(path.basename(parent.filename)),
          parentIdPath = isIndex ? parent.id : path.dirname(parent.id),
          id = path.join(parentIdPath, request);

      // make sure require('./path') and require('path') get distinct ids, even
      // when called from the toplevel js file
      if (parentIdPath === '.' && id.indexOf('/') === -1) {
        id = './' + id;
      }
      debug('RELATIVE: requested:' + request +
            ' set ID to: ' + id + ' from ' + parent.id);
      return [id, [path.dirname(parent.filename)]];
    }


    function loadModule(request, parent) {
      debug('loadModule REQUEST  ' + (request) + ' parent: ' + parent.id);

      var resolved = resolveModuleFilename(request, parent);
      var id = resolved[0];
      var filename = resolved[1];

      var cachedModule = moduleCache[filename];
      if (cachedModule) return cachedModule.exports;

      // With natives id === request
      // We deal with these first
      if (natives[id]) {
        // REPL is a special case, because it needs the real require.
        if (id == 'repl') {
          var replModule = new Module('repl');
          replModule._compile(natives.repl, 'repl.js');
          internalModuleCache.repl = replModule;
          return replModule.exports;
        }

        debug('load native module ' + request);
        return requireNative(id);
      }

      var module = new Module(id, parent);
      moduleCache[filename] = module;
      module.load(filename);
      return module.exports;
    };

    function resolveModuleFilename(request, parent) {
      if (natives[request]) return [request, request];
      var resolvedModule = resolveModuleLookupPaths(request, parent),
          id = resolvedModule[0],
          paths = resolvedModule[1];

      // look up the filename first, since that's the cache key.
      debug('looking for ' + JSON.stringify(id) +
            ' in ' + JSON.stringify(paths));
      var filename = findModulePath(request, paths);
      if (!filename) {
        throw new Error("Cannot find module '" + request + "'");
      }
      return [id, filename];
    }


    Module.prototype.load = function(filename) {
      debug('load ' + JSON.stringify(filename) +
            ' for module ' + JSON.stringify(this.id));

      process.assert(!this.loaded);
      this.filename = filename;

      var extension = path.extname(filename) || '.js';
      if (!extensions[extension]) extension = '.js';
      extensions[extension](this, filename);
      this.loaded = true;
    };


    // Returns exception if any
    Module.prototype._compile = function(content, filename) {
      var self = this;
      // remove shebang
      content = content.replace(/^\#\!.*/, '');

      function require(path) {
        return loadModule(path, self);
      }

      require.resolve = function(request) {
        return resolveModuleFilename(request, self)[1];
      }
      require.paths = modulePaths;
      require.main = process.mainModule;
      // Enable support to add extra extension types
      require.extensions = extensions;
      // TODO: Insert depreciation warning
      require.registerExtension = registerExtension;
      require.cache = moduleCache;

      var dirname = path.dirname(filename);

      if (contextLoad) {
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

          return evals.Script.runInNewContext(content, sandbox, filename);
        } else {
          debug('load root module');
          // root module
          global.require = require;
          global.exports = self.exports;
          global.__filename = filename;
          global.__dirname = dirname;
          global.module = self;

          return evals.Script.runInThisContext(content, filename);
        }

      } else {
        // create wrapper function
        var wrapper =
            '(function (exports, require, module, __filename, __dirname) { ' +
            content +
            '\n});';

        var compiledWrapper = evals.Script.runInThisContext(wrapper, filename);
        if (filename === process.argv[1] && global.v8debug) {
          global.v8debug.Debug.setBreakPoint(compiledWrapper, 0, 0);
        }
        var args = [self.exports, require, self, filename, dirname];
        return compiledWrapper.apply(self.exports, args);
      }
    };


    // Native extension for .js
    extensions['.js'] = function(module, filename) {
      var content = requireNative('fs').readFileSync(filename, 'utf8');
      module._compile(content, filename);
    };


    // Native extension for .node
    extensions['.node'] = function(module, filename) {
      process.dlopen(filename, module.exports);
    };


    // bootstrap main module.
    exports.runMain = function() {
      // Load the main module--the command line argument.
      process.mainModule = new Module('.');
      process.mainModule.load(process.argv[1]);
    };

    // bootstrap repl
    exports.requireRepl = function() { return loadModule('repl', '.'); };

    // export for --eval
    exports.Module = Module;

    return exports;
  })();


  // Load events module in order to access prototype elements on process like
  // process.addListener.
  var events = requireNative('events');

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
    var t = requireNative('timers');
    return t.setTimeout.apply(this, arguments);
  };

  global.setInterval = function() {
    var t = requireNative('timers');
    return t.setInterval.apply(this, arguments);
  };

  global.clearTimeout = function() {
    var t = requireNative('timers');
    return t.clearTimeout.apply(this, arguments);
  };

  global.clearInterval = function() {
    var t = requireNative('timers');
    return t.clearInterval.apply(this, arguments);
  };


  var stdout;
  process.__defineGetter__('stdout', function() {
    if (stdout) return stdout;

    var binding = process.binding('stdio'),
        net = requireNative('net'),
        fs = requireNative('fs'),
        fd = binding.stdoutFD;

    if (binding.isStdoutBlocking()) {
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

  var stdin;
  process.openStdin = function() {
    if (stdin) return stdin;

    var binding = process.binding('stdio'),
        net = requireNative('net'),
        fs = requireNative('fs'),
        fd = binding.openStdin();

    if (binding.isStdinBlocking()) {
      stdin = new fs.ReadStream(null, {fd: fd});
    } else {
      stdin = new net.Stream(fd);
      stdin.readable = true;
    }

    stdin.resume();

    return stdin;
  };


  // Lazy load console object
  global.__defineGetter__('console', function() {
    return requireNative('console');
  });


  global.Buffer = requireNative('buffer').Buffer;

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
  var path = requireNative('path');

  // Make process.argv[0] and process.argv[1] into full paths.
  if (process.argv[0].indexOf('/') > 0) {
    process.argv[0] = path.join(cwd, process.argv[0]);
  }

  if (process.argv[1]) {
    // Load module
    if (process.argv[1].charAt(0) != '/' &&
        !(/^http:\/\//).exec(process.argv[1])) {
      process.argv[1] = path.join(cwd, process.argv[1]);
    }
    // REMOVEME: nextTick should not be necessary. This hack to get
    // test/simple/test-exception-handler2.js working.
    process.nextTick(module.runMain);

  } else if (process._eval) {
    // -e, --eval
    var rv = new module.Module()._compile('return eval(process._eval)', 'eval');
    console.log(rv);
  } else {
    // REPL
    module.requireRepl().start();
  }

});
