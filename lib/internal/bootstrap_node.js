// Hello, and welcome to hacking node.js!
//
// This file is invoked by node::LoadEnvironment in src/node.cc, and is
// responsible for bootstrapping the node.js core. As special caution is given
// to the performance of the startup process, many dependencies are invoked
// lazily.

'use strict';

(function(process) {

  function startup() {
    const EventEmitter = NativeModule.require('events');
    process._eventsCount = 0;

    const origProcProto = Object.getPrototypeOf(process);
    Object.setPrototypeOf(process, Object.create(EventEmitter.prototype, {
      constructor: Object.getOwnPropertyDescriptor(origProcProto, 'constructor')
    }));

    EventEmitter.call(process);

    setupProcessObject();

    // do this good and early, since it handles errors.
    setupProcessFatal();

    setupProcessICUVersions();

    setupGlobalVariables();
    if (!process._noBrowserGlobals) {
      setupGlobalTimeouts();
      setupGlobalConsole();
    }

    const _process = NativeModule.require('internal/process');

    _process.setup_hrtime();
    _process.setup_cpuUsage();
    _process.setupMemoryUsage();
    _process.setupConfig(NativeModule._source);
    NativeModule.require('internal/process/warning').setup();
    NativeModule.require('internal/process/next_tick').setup();
    NativeModule.require('internal/process/stdio').setup();
    _process.setupKillAndExit();
    _process.setupSignalHandlers();
    if (global.__coverage__)
      NativeModule.require('internal/process/write-coverage').setup();

    // Do not initialize channel in debugger agent, it deletes env variable
    // and the main thread won't see it.
    if (process.argv[1] !== '--debug-agent')
      _process.setupChannel();

    _process.setupRawDebug();

    Object.defineProperty(process, 'argv0', {
      enumerable: true,
      configurable: false,
      value: process.argv[0]
    });
    process.argv[0] = process.execPath;

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

    } else if (process.argv[1] === 'debug') {
      // Start the debugger agent
      NativeModule.require('_debugger').start();

    } else if (process.argv[1] === 'inspect') {
      // Start the debugger agent
      process.nextTick(function() {
        NativeModule.require('node-inspect/lib/_inspect').start();
      });

    } else if (process.argv[1] === '--remote_debugging_server') {
      // Start the debugging server
      NativeModule.require('internal/inspector/remote_debugging_server');

    } else if (process.argv[1] === '--debug-agent') {
      // Start the debugger agent
      NativeModule.require('_debug_agent').start();

    } else if (process.profProcess) {
      NativeModule.require('internal/v8_prof_processor');

    } else {
      // There is user code to be run

      // If this is a worker in cluster mode, start up the communication
      // channel. This needs to be done before any user code gets executed
      // (including preload modules).
      if (process.argv[1] && process.env.NODE_UNIQUE_ID) {
        const cluster = NativeModule.require('cluster');
        cluster._setupWorker();

        // Make sure it's not accidentally inherited by child processes.
        delete process.env.NODE_UNIQUE_ID;
      }

      if (process._eval != null && !process._forceRepl) {
        // User passed '-e' or '--eval' arguments to Node without '-i' or
        // '--interactive'
        preloadModules();

        const internalModule = NativeModule.require('internal/module');
        internalModule.addBuiltinLibsToObject(global);
        run(() => {
          evalScript('[eval]');
        });
      } else if (process.argv[1]) {
        // make process.argv[1] into a full path
        const path = NativeModule.require('path');
        process.argv[1] = path.resolve(process.argv[1]);

        const Module = NativeModule.require('module');

        // check if user passed `-c` or `--check` arguments to Node.
        if (process._syntax_check_only != null) {
          const vm = NativeModule.require('vm');
          const fs = NativeModule.require('fs');
          const internalModule = NativeModule.require('internal/module');
          // read the source
          const filename = Module._resolveFilename(process.argv[1]);
          var source = fs.readFileSync(filename, 'utf-8');
          // remove shebang and BOM
          source = internalModule.stripBOM(source.replace(/^#!.*/, ''));
          // wrap it
          source = Module.wrap(source);
          // compile the script, this will throw if it fails
          new vm.Script(source, {filename: filename, displayErrors: true});
          process.exit(0);
        }

        preloadModules();
        run(Module.runMain);
      } else {
        preloadModules();
        // If -i or --interactive were passed, or stdin is a TTY.
        if (process._forceRepl || NativeModule.require('tty').isatty(0)) {
          // REPL
          const cliRepl = NativeModule.require('internal/repl');
          cliRepl.createInternalRepl(process.env, function(err, repl) {
            if (err) {
              throw err;
            }
            repl.on('exit', function() {
              if (repl._flushing) {
                repl.pause();
                return repl.once('flushHistory', function() {
                  process.exit();
                });
              }
              process.exit();
            });
          });

          if (process._eval != null) {
            // User passed '-e' or '--eval'
            evalScript('[eval]');
          }
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
  }

  function setupProcessObject() {
    process._setupProcessObject(pushValueToArray);

    function pushValueToArray() {
      for (var i = 0; i < arguments.length; i++)
        this.push(arguments[i]);
    }
  }

  function setupGlobalVariables() {
    Object.defineProperty(global, Symbol.toStringTag, {
      value: 'global',
      writable: false,
      enumerable: false,
      configurable: true
    });
    global.process = process;
    const util = NativeModule.require('util');

    function makeGetter(name) {
      return util.deprecate(function() {
        return this;
      }, `'${name}' is deprecated, use 'global'`, 'DEP0016');
    }

    function makeSetter(name) {
      return util.deprecate(function(value) {
        Object.defineProperty(this, name, {
          configurable: true,
          writable: true,
          enumerable: true,
          value: value
        });
      }, `'${name}' is deprecated, use 'global'`, 'DEP0016');
    }

    Object.defineProperties(global, {
      GLOBAL: {
        configurable: true,
        get: makeGetter('GLOBAL'),
        set: makeSetter('GLOBAL')
      },
      root: {
        configurable: true,
        get: makeGetter('root'),
        set: makeSetter('root')
      }
    });

    global.Buffer = NativeModule.require('buffer').Buffer;
    process.domain = null;
    process._exiting = false;
  }

  function setupGlobalTimeouts() {
    const timers = NativeModule.require('timers');
    global.clearImmediate = timers.clearImmediate;
    global.clearInterval = timers.clearInterval;
    global.clearTimeout = timers.clearTimeout;
    global.setImmediate = timers.setImmediate;
    global.setInterval = timers.setInterval;
    global.setTimeout = timers.setTimeout;
  }

  function setupGlobalConsole() {
    var inspectorConsole;
    var wrapConsoleCall;
    if (process.inspector) {
      inspectorConsole = global.console;
      wrapConsoleCall = process.inspector.wrapConsoleCall;
      delete process.inspector;
    }
    var console;
    Object.defineProperty(global, 'console', {
      configurable: true,
      enumerable: true,
      get: function() {
        if (!console) {
          console = NativeModule.require('console');
          installInspectorConsoleIfNeeded(console,
                                          inspectorConsole,
                                          wrapConsoleCall);
        }
        return console;
      }
    });
  }

  function installInspectorConsoleIfNeeded(console,
                                           inspectorConsole,
                                           wrapConsoleCall) {
    if (!inspectorConsole)
      return;
    const config = {};
    for (const key of Object.keys(console)) {
      if (!inspectorConsole.hasOwnProperty(key))
        continue;
      // If node console has the same method as inspector console,
      // then wrap these two methods into one. Native wrapper will preserve
      // the original stack.
      console[key] = wrapConsoleCall(inspectorConsole[key],
                                     console[key],
                                     config);
    }
    for (const key of Object.keys(inspectorConsole)) {
      if (console.hasOwnProperty(key))
        continue;
      console[key] = inspectorConsole[key];
    }
  }

  function setupProcessFatal() {

    process._fatalException = function(er) {
      var caught;

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
        NativeModule.require('timers').setImmediate(process._tickCallback);
      }

      return caught;
    };
  }

  function setupProcessICUVersions() {
    const icu = process.binding('config').hasIntl ?
      process.binding('icu') : undefined;
    if (!icu) return;  // no Intl/ICU: nothing to add here.
    // With no argument, getVersion() returns a comma separated list
    // of possible types.
    const versionTypes = icu.getVersion().split(',');

    function makeGetter(name) {
      return () => {
        // With an argument, getVersion(type) returns
        // the actual version string.
        const version = icu.getVersion(name);
        // Replace the current getter with a new property.
        delete process.versions[name];
        Object.defineProperty(process.versions, name, {
          value: version,
          writable: false,
          enumerable: true
        });
        return version;
      };
    }

    for (var n = 0; n < versionTypes.length; n++) {
      var name = versionTypes[n];
      Object.defineProperty(process.versions, name, {
        configurable: true,
        enumerable: true,
        get: makeGetter(name)
      });
    }
  }

  function tryGetCwd(path) {
    var threw = true;
    var cwd;
    try {
      cwd = process.cwd();
      threw = false;
    } finally {
      if (threw) {
        // getcwd(3) can fail if the current working directory has been deleted.
        // Fall back to the directory name of the (absolute) executable path.
        // It's not really correct but what are the alternatives?
        return path.dirname(process.execPath);
      }
    }
    return cwd;
  }

  function evalScript(name) {
    const Module = NativeModule.require('module');
    const path = NativeModule.require('path');
    const cwd = tryGetCwd(path);

    const module = new Module(name);
    module.filename = path.join(cwd, name);
    module.paths = Module._nodeModulePaths(cwd);
    const body = process._eval;
    const script = `global.__filename = ${JSON.stringify(name)};\n` +
                   'global.exports = exports;\n' +
                   'global.module = module;\n' +
                   'global.__dirname = __dirname;\n' +
                   'global.require = require;\n' +
                   'return require("vm").runInThisContext(' +
                   `${JSON.stringify(body)}, { filename: ` +
                   `${JSON.stringify(name)}, displayErrors: true });\n`;
    const result = module._compile(script, `${name}-wrapper`);
    if (process._print_eval) console.log(result);
    process._tickCallback();
  }

  // Load preload modules
  function preloadModules() {
    if (process._preload_modules) {
      NativeModule.require('module')._preloadModules(process._preload_modules);
    }
  }

  function isDebugBreak() {
    return process.execArgv.some((arg) => /^--debug-brk(=[0-9]+)?$/.test(arg));
  }

  function run(entryFunction) {
    if (process._debugWaitConnect && isDebugBreak()) {

      // XXX Fix this terrible hack!
      //
      // Give the client program a few ticks to connect.
      // Otherwise, there's a race condition where `node debug foo.js`
      // will not be able to connect in time to catch the first
      // breakpoint message on line 1.
      //
      // A better fix would be to somehow get a message from the
      // V8 debug object about a connection, and runMain when
      // that occurs.  --isaacs

      const debugTimeout = +process.env.NODE_DEBUG_TIMEOUT || 50;
      setTimeout(entryFunction, debugTimeout);

    } else {
      // Main entry point into most programs:
      entryFunction();
    }
  }

  // Below you find a minimal module system, which is used to load the node
  // core modules found in lib/*.js. All core modules are compiled into the
  // node binary, so they can be loaded faster.

  const ContextifyScript = process.binding('contextify').ContextifyScript;
  function runInThisContext(code, options) {
    const script = new ContextifyScript(code, options);
    return script.runInThisContext();
  }

  function NativeModule(id) {
    this.filename = `${id}.js`;
    this.id = id;
    this.exports = {};
    this.loaded = false;
    this.loading = false;
  }

  NativeModule._source = process.binding('natives');
  NativeModule._cache = {};

  NativeModule.require = function(id) {
    if (id === 'native_module') {
      return NativeModule;
    }

    const cached = NativeModule.getCached(id);
    if (cached && (cached.loaded || cached.loading)) {
      return cached.exports;
    }

    if (!NativeModule.exists(id)) {
      throw new Error(`No such native module ${id}`);
    }

    process.moduleLoadList.push(`NativeModule ${id}`);

    const nativeModule = new NativeModule(id);

    nativeModule.cache();
    nativeModule.compile();

    return nativeModule.exports;
  };

  NativeModule.getCached = function(id) {
    return NativeModule._cache[id];
  };

  NativeModule.exists = function(id) {
    return NativeModule._source.hasOwnProperty(id);
  };

  const EXPOSE_INTERNALS = process.execArgv.some(function(arg) {
    return arg.match(/^--expose[-_]internals$/);
  });

  if (EXPOSE_INTERNALS) {
    NativeModule.nonInternalExists = NativeModule.exists;

    NativeModule.isInternal = function(id) {
      return false;
    };
  } else {
    NativeModule.nonInternalExists = function(id) {
      return NativeModule.exists(id) && !NativeModule.isInternal(id);
    };

    NativeModule.isInternal = function(id) {
      return id.startsWith('internal/');
    };
  }


  NativeModule.getSource = function(id) {
    return NativeModule._source[id];
  };

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

    this.loading = true;

    try {
      const fn = runInThisContext(source, {
        filename: this.filename,
        lineOffset: 0,
        displayErrors: true
      });
      fn(this.exports, NativeModule.require, this, this.filename);

      this.loaded = true;
    } finally {
      this.loading = false;
    }
  };

  NativeModule.prototype.cache = function() {
    NativeModule._cache[this.id] = this;
  };

  startup();
});
