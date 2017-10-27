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
    Object.setPrototypeOf(origProcProto, EventEmitter.prototype);

    EventEmitter.call(process);

    setupProcessObject();

    // do this good and early, since it handles errors.
    setupProcessFatal();

    setupProcessICUVersions();

    setupGlobalVariables();

    const _process = NativeModule.require('internal/process');
    _process.setupConfig(NativeModule._source);
    NativeModule.require('internal/process/warning').setup();
    NativeModule.require('internal/process/next_tick').setup();
    NativeModule.require('internal/process/stdio').setup();

    const perf = process.binding('performance');
    const {
      NODE_PERFORMANCE_MILESTONE_BOOTSTRAP_COMPLETE,
      NODE_PERFORMANCE_MILESTONE_THIRD_PARTY_MAIN_START,
      NODE_PERFORMANCE_MILESTONE_THIRD_PARTY_MAIN_END,
      NODE_PERFORMANCE_MILESTONE_CLUSTER_SETUP_START,
      NODE_PERFORMANCE_MILESTONE_CLUSTER_SETUP_END,
      NODE_PERFORMANCE_MILESTONE_MODULE_LOAD_START,
      NODE_PERFORMANCE_MILESTONE_MODULE_LOAD_END,
      NODE_PERFORMANCE_MILESTONE_PRELOAD_MODULE_LOAD_START,
      NODE_PERFORMANCE_MILESTONE_PRELOAD_MODULE_LOAD_END
    } = perf.constants;

    _process.setup_hrtime();
    _process.setup_performance();
    _process.setup_cpuUsage();
    _process.setupMemoryUsage();
    _process.setupKillAndExit();
    _process.setupSignalHandlers();
    if (global.__coverage__)
      NativeModule.require('internal/process/write-coverage').setup();

    NativeModule.require('internal/inspector_async_hook').setup();

    // Do not initialize channel in debugger agent, it deletes env variable
    // and the main thread won't see it.
    if (process.argv[1] !== '--debug-agent')
      _process.setupChannel();

    _process.setupRawDebug();

    const browserGlobals = !process._noBrowserGlobals;
    if (browserGlobals) {
      setupGlobalTimeouts();
      setupGlobalConsole();
    }

    // Ensure setURLConstructor() is called before the native
    // URL::ToObject() method is used.
    NativeModule.require('internal/url');

    Object.defineProperty(process, 'argv0', {
      enumerable: true,
      configurable: false,
      value: process.argv[0]
    });
    process.argv[0] = process.execPath;

    // Handle `--debug*` deprecation and invalidation
    if (process._invalidDebug) {
      process.emitWarning(
        '`node --debug` and `node --debug-brk` are invalid. ' +
        'Please use `node --inspect` or `node --inspect-brk` instead.',
        'DeprecationWarning', 'DEP0062', startup, true);
      process.exit(9);
    } else if (process._deprecatedDebugBrk) {
      process.emitWarning(
        '`node --inspect --debug-brk` is deprecated. ' +
        'Please use `node --inspect-brk` instead.',
        'DeprecationWarning', 'DEP0062', startup, true);
    }

    if (process.binding('config').experimentalModules) {
      process.emitWarning(
        'The ESM module loader is experimental.',
        'ExperimentalWarning', undefined);
    }


    // There are various modes that Node can run in. The most common two
    // are running from a script and running the REPL - but there are a few
    // others like the debugger or running --eval arguments. Here we decide
    // which mode we run in.

    if (NativeModule.exists('_third_party_main')) {
      // To allow people to extend Node in different ways, this hook allows
      // one to drop a file lib/_third_party_main.js into the build
      // directory which will be executed instead of Node's normal loading.
      process.nextTick(function() {
        perf.markMilestone(NODE_PERFORMANCE_MILESTONE_THIRD_PARTY_MAIN_START);
        NativeModule.require('_third_party_main');
        perf.markMilestone(NODE_PERFORMANCE_MILESTONE_THIRD_PARTY_MAIN_END);
      });

    } else if (process.argv[1] === 'inspect' || process.argv[1] === 'debug') {
      if (process.argv[1] === 'debug') {
        process.emitWarning(
          '`node debug` is deprecated. Please use `node inspect` instead.',
          'DeprecationWarning', 'DEP0068');
      }

      // Start the debugger agent
      process.nextTick(function() {
        NativeModule.require('node-inspect/lib/_inspect').start();
      });

    } else if (process.profProcess) {
      NativeModule.require('internal/v8_prof_processor');

    } else {
      // There is user code to be run

      // If this is a worker in cluster mode, start up the communication
      // channel. This needs to be done before any user code gets executed
      // (including preload modules).
      if (process.argv[1] && process.env.NODE_UNIQUE_ID) {
        perf.markMilestone(NODE_PERFORMANCE_MILESTONE_CLUSTER_SETUP_START);
        const cluster = NativeModule.require('cluster');
        cluster._setupWorker();
        perf.markMilestone(NODE_PERFORMANCE_MILESTONE_CLUSTER_SETUP_END);
        // Make sure it's not accidentally inherited by child processes.
        delete process.env.NODE_UNIQUE_ID;
      }

      if (process._eval != null && !process._forceRepl) {
        perf.markMilestone(NODE_PERFORMANCE_MILESTONE_MODULE_LOAD_START);
        perf.markMilestone(NODE_PERFORMANCE_MILESTONE_MODULE_LOAD_END);
        // User passed '-e' or '--eval' arguments to Node without '-i' or
        // '--interactive'

        perf.markMilestone(
          NODE_PERFORMANCE_MILESTONE_PRELOAD_MODULE_LOAD_START);
        preloadModules();
        perf.markMilestone(NODE_PERFORMANCE_MILESTONE_PRELOAD_MODULE_LOAD_END);

        const internalModule = NativeModule.require('internal/module');
        internalModule.addBuiltinLibsToObject(global);
        evalScript('[eval]');
      } else if (process.argv[1] && process.argv[1] !== '-') {
        perf.markMilestone(NODE_PERFORMANCE_MILESTONE_MODULE_LOAD_START);
        // make process.argv[1] into a full path
        const path = NativeModule.require('path');
        process.argv[1] = path.resolve(process.argv[1]);

        const Module = NativeModule.require('module');

        // check if user passed `-c` or `--check` arguments to Node.
        if (process._syntax_check_only != null) {
          const fs = NativeModule.require('fs');
          // read the source
          const filename = Module._resolveFilename(process.argv[1]);
          var source = fs.readFileSync(filename, 'utf-8');
          checkScriptSyntax(source, filename);
          process.exit(0);
        }
        perf.markMilestone(NODE_PERFORMANCE_MILESTONE_MODULE_LOAD_END);
        perf.markMilestone(
          NODE_PERFORMANCE_MILESTONE_PRELOAD_MODULE_LOAD_START);
        preloadModules();
        perf.markMilestone(
          NODE_PERFORMANCE_MILESTONE_PRELOAD_MODULE_LOAD_END);
        Module.runMain();
      } else {
        perf.markMilestone(NODE_PERFORMANCE_MILESTONE_MODULE_LOAD_START);
        perf.markMilestone(NODE_PERFORMANCE_MILESTONE_MODULE_LOAD_END);
        perf.markMilestone(
          NODE_PERFORMANCE_MILESTONE_PRELOAD_MODULE_LOAD_START);
        preloadModules();
        perf.markMilestone(
          NODE_PERFORMANCE_MILESTONE_PRELOAD_MODULE_LOAD_END);
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
            if (process._syntax_check_only != null) {
              checkScriptSyntax(code, '[stdin]');
            } else {
              process._eval = code;
              evalScript('[stdin]');
            }
          });
        }
      }
    }
    perf.markMilestone(NODE_PERFORMANCE_MILESTONE_BOOTSTRAP_COMPLETE);
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
    const originalConsole = global.console;
    const Module = NativeModule.require('module');
    // Setup Node.js global.console
    const wrappedConsole = NativeModule.require('console');
    Object.defineProperty(global, 'console', {
      configurable: true,
      enumerable: true,
      get() {
        return wrappedConsole;
      }
    });
    setupInspector(originalConsole, wrappedConsole, Module);
  }

  function setupInspector(originalConsole, wrappedConsole, Module) {
    const { addCommandLineAPI, consoleCall } = process.binding('inspector');
    if (!consoleCall) {
      return;
    }
    // Setup inspector command line API
    const { makeRequireFunction } = NativeModule.require('internal/module');
    const path = NativeModule.require('path');
    const cwd = tryGetCwd(path);

    const consoleAPIModule = new Module('<inspector console>');
    consoleAPIModule.paths =
        Module._nodeModulePaths(cwd).concat(Module.globalPaths);
    addCommandLineAPI('require', makeRequireFunction(consoleAPIModule));
    const config = {};
    for (const key of Object.keys(wrappedConsole)) {
      if (!originalConsole.hasOwnProperty(key))
        continue;
      // If global console has the same method as inspector console,
      // then wrap these two methods into one. Native wrapper will preserve
      // the original stack.
      wrappedConsole[key] = consoleCall.bind(wrappedConsole,
                                             originalConsole[key],
                                             wrappedConsole[key],
                                             config);
    }
    for (const key of Object.keys(originalConsole)) {
      if (wrappedConsole.hasOwnProperty(key))
        continue;
      wrappedConsole[key] = originalConsole[key];
    }
  }

  function setupProcessFatal() {
    const async_wrap = process.binding('async_wrap');
    // Arrays containing hook flags and ids for async_hook calls.
    const { async_hook_fields, async_id_fields } = async_wrap;
    // Internal functions needed to manipulate the stack.
    const { clearAsyncIdStack, asyncIdStackSize } = async_wrap;
    const { kAfter, kExecutionAsyncId,
            kInitTriggerAsyncId } = async_wrap.constants;

    process._fatalException = function(er) {
      var caught;

      // It's possible that kInitTriggerAsyncId was set for a constructor call
      // that threw and was never cleared. So clear it now.
      async_id_fields[kInitTriggerAsyncId] = 0;

      if (process.domain && process.domain._errorHandler)
        caught = process.domain._errorHandler(er);

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

      } else {
        // If we handled an error, then make sure any ticks get processed
        NativeModule.require('timers').setImmediate(process._tickCallback);

        // Emit the after() hooks now that the exception has been handled.
        if (async_hook_fields[kAfter] > 0) {
          do {
            NativeModule.require('async_hooks').emitAfter(
              async_id_fields[kExecutionAsyncId]);
          } while (asyncIdStackSize() > 0);
        // Or completely empty the id stack.
        } else {
          clearAsyncIdStack();
        }
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

    for (var n = 0; n < versionTypes.length; n++) {
      var name = versionTypes[n];
      const version = icu.getVersion(name);
      Object.defineProperty(process.versions, name, {
        writable: false,
        enumerable: true,
        value: version
      });
    }
  }

  function tryGetCwd(path) {
    try {
      return process.cwd();
    } catch (ex) {
      // getcwd(3) can fail if the current working directory has been deleted.
      // Fall back to the directory name of the (absolute) executable path.
      // It's not really correct but what are the alternatives?
      return path.dirname(process.execPath);
    }
  }

  function wrapForBreakOnFirstLine(source) {
    if (!process._breakFirstLine)
      return source;
    const fn = `function() {\n\n${source};\n\n}`;
    return `process.binding('inspector').callAndPauseOnStart(${fn}, {})`;
  }

  function evalScript(name) {
    const Module = NativeModule.require('module');
    const path = NativeModule.require('path');
    const cwd = tryGetCwd(path);

    const module = new Module(name);
    module.filename = path.join(cwd, name);
    module.paths = Module._nodeModulePaths(cwd);
    const body = wrapForBreakOnFirstLine(process._eval);
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
    // Handle any nextTicks added in the first tick of the program.
    process._tickCallback();
  }

  // Load preload modules
  function preloadModules() {
    if (process._preload_modules) {
      NativeModule.require('module')._preloadModules(process._preload_modules);
    }
  }

  function checkScriptSyntax(source, filename) {
    const Module = NativeModule.require('module');
    const vm = NativeModule.require('vm');
    const internalModule = NativeModule.require('internal/module');

    // remove Shebang
    source = internalModule.stripShebang(source);
    // remove BOM
    source = internalModule.stripBOM(source);
    // wrap it
    source = Module.wrap(source);
    // compile the script, this will throw if it fails
    new vm.Script(source, { displayErrors: true, filename });
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

  const config = process.binding('config');

  if (!config.exposeHTTP2)
    delete NativeModule._source.http2;

  NativeModule.require = function(id) {
    if (id === 'native_module') {
      return NativeModule;
    }

    const cached = NativeModule.getCached(id);
    if (cached && (cached.loaded || cached.loading)) {
      return cached.exports;
    }

    if (!NativeModule.exists(id)) {
      // Model the error off the internal/errors.js model, but
      // do not use that module given that it could actually be
      // the one causing the error if there's a bug in Node.js
      const err = new Error(`No such built-in module: ${id}`);
      err.code = 'ERR_UNKNOWN_BUILTIN_MODULE';
      err.name = 'Error [ERR_UNKNOWN_BUILTIN_MODULE]';
      throw err;
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

  if (config.exposeInternals) {
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
