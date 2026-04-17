'use strict';

const {
  ArrayPrototypeForEach,
  ArrayPrototypeJoin,
  ArrayPrototypeMap,
  ArrayPrototypePop,
  ArrayPrototypeShift,
  ArrayPrototypeSlice,
  FunctionPrototypeBind,
  Number,
  PromisePrototypeThen,
  PromiseResolve,
  Proxy,
  RegExpPrototypeExec,
  RegExpPrototypeSymbolSplit,
  StringPrototypeEndsWith,
  StringPrototypeSplit,
} = primordials;

const { EventEmitter } = require('events');
const util = require('util');
const {
  setTimeout: pSetTimeout,
} = require('timers/promises');

const InspectClient = require('internal/debugger/inspect_client');
const {
  launchChildProcess,
  writeUsageAndExit,
} = require('internal/debugger/inspect_helpers');
const {
  startProbeMode,
} = require('internal/debugger/inspect_probe');
const createRepl = require('internal/debugger/inspect_repl');

const debuglog = util.debuglog('inspect');

const {
  exitCodes: {
    kGenericUserError,
    kNoFailure,
  },
} = internalBinding('errors');

function createAgentProxy(domain, client) {
  const agent = new EventEmitter();
  agent.then = (then, _catch) => {
    // TODO: potentially fetch the protocol and pretty-print it here.
    const descriptor = {
      [util.inspect.custom](depth, { stylize }) {
        return stylize(`[Agent ${domain}]`, 'special');
      },
    };
    return PromisePrototypeThen(PromiseResolve(descriptor), then, _catch);
  };

  return new Proxy(agent, {
    __proto__: null,
    get(target, name) {
      if (name in target) return target[name];
      return function callVirtualMethod(params) {
        return client.callMethod(`${domain}.${name}`, params);
      };
    },
  });
}

class NodeInspector {
  constructor(options, stdin, stdout) {
    this.options = options;
    this.stdin = stdin;
    this.stdout = stdout;

    this.paused = true;
    this.child = null;

    if (options.script) {
      this._runScript = FunctionPrototypeBind(
        launchChildProcess, null,
        [options.script, ...options.scriptArgs],
        options.host,
        options.port,
        FunctionPrototypeBind(this.childPrint, this));
    } else {
      this._runScript =
          () => PromiseResolve([null, options.port, options.host]);
    }

    this.client = new InspectClient();

    this.domainNames = ['Debugger', 'HeapProfiler', 'Profiler', 'Runtime'];
    ArrayPrototypeForEach(this.domainNames, (domain) => {
      this[domain] = createAgentProxy(domain, this.client);
    });
    this.handleDebugEvent = (fullName, params) => {
      const { 0: domain, 1: name } = StringPrototypeSplit(fullName, '.', 2);
      if (domain in this) {
        this[domain].emit(name, params);
      }
    };
    this.client.on('debugEvent', this.handleDebugEvent);
    const startRepl = createRepl(this);

    // Handle all possible exits
    process.on('exit', () => this.killChild());
    const exitCodeZero = () => process.exit(kNoFailure);
    process.once('SIGTERM', exitCodeZero);
    process.once('SIGHUP', exitCodeZero);

    (async () => {
      try {
        await this.run();
        const repl = await startRepl();
        this.repl = repl;
        this.repl.on('exit', exitCodeZero);
        this.paused = false;
      } catch (error) {
        process.nextTick(() => { throw error; });
      }
    })();
  }

  suspendReplWhile(fn) {
    if (this.repl) {
      this.repl.pause();
    }
    this.stdin.pause();
    this.paused = true;
    return (async () => {
      try {
        await fn();
        this.paused = false;
        if (this.repl) {
          this.repl.resume();
          this.repl.displayPrompt();
        }
        this.stdin.resume();
      } catch (error) {
        process.nextTick(() => { throw error; });
      }
    })();
  }

  killChild() {
    this.client.reset();
    if (this.child) {
      this.child.kill();
      this.child = null;
    }
  }

  async run() {
    this.killChild();

    const { 0: child, 1: port, 2: host } = await this._runScript();
    this.child = child;

    this.print(`connecting to ${host}:${port} ..`, false);
    for (let attempt = 0; attempt < 5; attempt++) {
      debuglog('connection attempt #%d', attempt);
      this.stdout.write('.');
      try {
        await this.client.connect(port, host);
        debuglog('connection established');
        this.stdout.write(' ok\n');
        return;
      } catch (error) {
        debuglog('connect failed', error);
        await pSetTimeout(1000);
      }
    }
    this.stdout.write(' failed to connect, please retry\n');
    process.exit(kGenericUserError);
  }

  clearLine() {
    if (this.stdout.isTTY) {
      this.stdout.cursorTo(0);
      this.stdout.clearLine(1);
    } else {
      this.stdout.write('\b');
    }
  }

  print(text, appendNewline = false) {
    this.clearLine();
    this.stdout.write(appendNewline ? `${text}\n` : text);
  }

  #stdioBuffers = { stdout: '', stderr: '' };
  childPrint(text, which) {
    const lines = RegExpPrototypeSymbolSplit(
      /\r\n|\r|\n/g,
      this.#stdioBuffers[which] + text);

    this.#stdioBuffers[which] = '';

    if (lines[lines.length - 1] !== '') {
      this.#stdioBuffers[which] = ArrayPrototypePop(lines);
    }

    const textToPrint = ArrayPrototypeJoin(
      ArrayPrototypeMap(lines, (chunk) => `< ${chunk}`),
      '\n');

    if (lines.length) {
      this.print(textToPrint, true);
      if (!this.paused) {
        this.repl.displayPrompt(true);
      }
    }

    if (StringPrototypeEndsWith(
      textToPrint,
      'Waiting for the debugger to disconnect...\n',
    )) {
      this.killChild();
    }
  }
}

function parseArgv(args) {
  const target = ArrayPrototypeShift(args);
  let host = '127.0.0.1';
  let port = 9229;
  let isRemote = false;
  let script = target;
  let scriptArgs = args;

  const hostMatch = RegExpPrototypeExec(/^([^:]+):(\d+)$/, target);
  const portMatch = RegExpPrototypeExec(/^--port=(\d+)$/, target);

  if (hostMatch) {
    // Connecting to remote debugger
    host = hostMatch[1];
    port = Number(hostMatch[2]);
    isRemote = true;
    script = null;
  } else if (portMatch) {
    // Start on custom port
    port = Number(portMatch[1]);
    script = args[0];
    scriptArgs = ArrayPrototypeSlice(args, 1);
  } else if (args.length === 1 && RegExpPrototypeExec(/^\d+$/, args[0]) !== null &&
             target === '-p') {
    // Start debugger against a given pid
    const pid = Number(args[0]);
    try {
      process._debugProcess(pid);
    } catch (e) {
      if (e.code === 'ESRCH') {
        process.stderr.write(`Target process: ${pid} doesn't exist.\n`);
        process.exit(kGenericUserError);
      }
      throw e;
    }
    script = null;
    isRemote = true;
  }

  return {
    host, port, isRemote, script, scriptArgs,
  };
}

function startInspect(argv = ArrayPrototypeSlice(process.argv, 2),
                      stdin = process.stdin,
                      stdout = process.stdout) {
  const invokedAs = `${process.argv0} ${process.argv[1]}`;

  if (argv.length < 1) {
    writeUsageAndExit(invokedAs);
  }

  if (startProbeMode(invokedAs, argv, stdout)) {
    return;
  }

  const options = parseArgv(argv);
  const inspector = new NodeInspector(options, stdin, stdout);

  stdin.resume();

  function handleUnexpectedError(e) {
    if (e.code !== 'ERR_DEBUGGER_STARTUP_ERROR') {
      process.stderr.write('There was an internal error in Node.js. ' +
                           'Please report this bug.\n' +
                           `${e.message}\n${e.stack}\n`);
    } else {
      process.stderr.write(e.message);
      process.stderr.write('\n');
    }
    if (inspector.child) inspector.child.kill();
    process.exit(kGenericUserError);
  }

  process.on('uncaughtException', handleUnexpectedError);
}
exports.start = startInspect;
