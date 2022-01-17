'use strict';

const {
  ArrayPrototypeConcat,
  ArrayPrototypeForEach,
  ArrayPrototypeJoin,
  ArrayPrototypeMap,
  ArrayPrototypePop,
  ArrayPrototypeShift,
  ArrayPrototypeSlice,
  FunctionPrototypeBind,
  Number,
  Promise,
  PromisePrototypeCatch,
  PromisePrototypeThen,
  PromiseResolve,
  Proxy,
  RegExpPrototypeSymbolMatch,
  RegExpPrototypeSymbolSplit,
  RegExpPrototypeTest,
  StringPrototypeEndsWith,
  StringPrototypeSplit,
} = primordials;

const { spawn } = require('child_process');
const { EventEmitter } = require('events');
const net = require('net');
const util = require('util');
const {
  setInterval: pSetInterval,
  setTimeout: pSetTimeout,
} = require('timers/promises');
const {
  AbortController,
} = require('internal/abort_controller');

// TODO(aduh95): remove console calls
const console = require('internal/console/global');

const { 0: InspectClient, 1: createRepl } =
    [
      require('internal/debugger/inspect_client'),
      require('internal/debugger/inspect_repl'),
    ];

const debuglog = util.debuglog('inspect');

const { ERR_DEBUGGER_STARTUP_ERROR } = require('internal/errors').codes;

async function portIsFree(host, port, timeout = 9999) {
  if (port === 0) return; // Binding to a random port.

  const retryDelay = 150;
  const ac = new AbortController();
  const { signal } = ac;

  pSetTimeout(timeout).then(() => ac.abort());

  const asyncIterator = pSetInterval(retryDelay);
  while (true) {
    await asyncIterator.next();
    if (signal.aborted) {
      throw new ERR_DEBUGGER_STARTUP_ERROR(
        `Timeout (${timeout}) waiting for ${host}:${port} to be free`);
    }
    const error = await new Promise((resolve) => {
      const socket = net.connect(port, host);
      socket.on('error', resolve);
      socket.on('connect', resolve);
    });
    if (error?.code === 'ECONNREFUSED') {
      return;
    }
  }
}

const debugRegex = /Debugger listening on ws:\/\/\[?(.+?)\]?:(\d+)\//;
async function runScript(script, scriptArgs, inspectHost, inspectPort,
                         childPrint) {
  await portIsFree(inspectHost, inspectPort);
  const args = ArrayPrototypeConcat(
    [`--inspect-brk=${inspectPort}`, script],
    scriptArgs);
  const child = spawn(process.execPath, args);
  child.stdout.setEncoding('utf8');
  child.stderr.setEncoding('utf8');
  child.stdout.on('data', (chunk) => childPrint(chunk, 'stdout'));
  child.stderr.on('data', (chunk) => childPrint(chunk, 'stderr'));

  let output = '';
  return new Promise((resolve) => {
    function waitForListenHint(text) {
      output += text;
      const debug = RegExpPrototypeSymbolMatch(debugRegex, output);
      if (debug) {
        const host = debug[1];
        const port = Number(debug[2]);
        child.stderr.removeListener('data', waitForListenHint);
        resolve([child, port, host]);
      }
    }

    child.stderr.on('data', waitForListenHint);
  });
}

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
        runScript, null,
        options.script,
        options.scriptArgs,
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
      const { 0: domain, 1: name } = StringPrototypeSplit(fullName, '.');
      if (domain in this) {
        this[domain].emit(name, params);
      }
    };
    this.client.on('debugEvent', this.handleDebugEvent);
    const startRepl = createRepl(this);

    // Handle all possible exits
    process.on('exit', () => this.killChild());
    const exitCodeZero = () => process.exit(0);
    process.once('SIGTERM', exitCodeZero);
    process.once('SIGHUP', exitCodeZero);

    PromisePrototypeCatch(PromisePrototypeThen(this.run(), async () => {
      const repl = await startRepl();
      this.repl = repl;
      this.repl.on('exit', exitCodeZero);
      this.paused = false;
    }), (error) => process.nextTick(() => { throw error; }));
  }

  suspendReplWhile(fn) {
    if (this.repl) {
      this.repl.pause();
    }
    this.stdin.pause();
    this.paused = true;
    return PromisePrototypeCatch(PromisePrototypeThen(new Promise((resolve) => {
      resolve(fn());
    }), () => {
      this.paused = false;
      if (this.repl) {
        this.repl.resume();
        this.repl.displayPrompt();
      }
      this.stdin.resume();
    }), (error) => process.nextTick(() => { throw error; }));
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
    process.exit(1);
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
      'Waiting for the debugger to disconnect...\n'
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

  const hostMatch = RegExpPrototypeSymbolMatch(/^([^:]+):(\d+)$/, target);
  const portMatch = RegExpPrototypeSymbolMatch(/^--port=(\d+)$/, target);

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
  } else if (args.length === 1 && RegExpPrototypeTest(/^\d+$/, args[0]) &&
             target === '-p') {
    // Start debugger against a given pid
    const pid = Number(args[0]);
    try {
      process._debugProcess(pid);
    } catch (e) {
      if (e.code === 'ESRCH') {
        console.error(`Target process: ${pid} doesn't exist.`);
        process.exit(1);
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
  if (argv.length < 1) {
    const invokedAs = `${process.argv0} ${process.argv[1]}`;

    console.error(`Usage: ${invokedAs} script.js`);
    console.error(`       ${invokedAs} <host>:<port>`);
    console.error(`       ${invokedAs} --port=<port>`);
    console.error(`       ${invokedAs} -p <pid>`);
    process.exit(1);
  }

  const options = parseArgv(argv);
  const inspector = new NodeInspector(options, stdin, stdout);

  stdin.resume();

  function handleUnexpectedError(e) {
    if (e.code !== 'ERR_DEBUGGER_STARTUP_ERROR') {
      console.error('There was an internal error in Node.js. ' +
                    'Please report this bug.');
      console.error(e.message);
      console.error(e.stack);
    } else {
      console.error(e.message);
    }
    if (inspector.child) inspector.child.kill();
    process.exit(1);
  }

  process.on('uncaughtException', handleUnexpectedError);
}
exports.start = startInspect;
