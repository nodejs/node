/*
 * Copyright Node.js contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
'use strict';
const { spawn } = require('child_process');
const { EventEmitter } = require('events');
const net = require('net');
const util = require('util');

const runAsStandalone = typeof __dirname !== 'undefined';

const [ InspectClient, createRepl ] =
  runAsStandalone ?
  // This copy of node-inspect is on-disk, relative paths make sense.
  [
    require('./internal/inspect_client'),
    require('./internal/inspect_repl')
  ] :
  // This copy of node-inspect is built into the node executable.
  [
    require('node-inspect/lib/internal/inspect_client'),
    require('node-inspect/lib/internal/inspect_repl')
  ];

const debuglog = util.debuglog('inspect');

class StartupError extends Error {
  constructor(message) {
    super(message);
    this.name = 'StartupError';
  }
}

function portIsFree(host, port, timeout = 2000) {
  if (port === 0) return Promise.resolve();  // Binding to a random port.

  const retryDelay = 150;
  let didTimeOut = false;

  return new Promise((resolve, reject) => {
    setTimeout(() => {
      didTimeOut = true;
      reject(new StartupError(
        `Timeout (${timeout}) waiting for ${host}:${port} to be free`));
    }, timeout);

    function pingPort() {
      if (didTimeOut) return;

      const socket = net.connect(port, host);
      let didRetry = false;
      function retry() {
        if (!didRetry && !didTimeOut) {
          didRetry = true;
          setTimeout(pingPort, retryDelay);
        }
      }

      socket.on('error', (error) => {
        if (error.code === 'ECONNREFUSED') {
          resolve();
        } else {
          retry();
        }
      });
      socket.on('connect', () => {
        socket.destroy();
        retry();
      });
    }
    pingPort();
  });
}

function runScript(script, scriptArgs, inspectHost, inspectPort, childPrint) {
  return portIsFree(inspectHost, inspectPort)
    .then(() => {
      return new Promise((resolve) => {
        const needDebugBrk = process.version.match(/^v(6|7)\./);
        const args = (needDebugBrk ?
                          ['--inspect', `--debug-brk=${inspectPort}`] :
                          [`--inspect-brk=${inspectPort}`])
                         .concat([script], scriptArgs);
        const child = spawn(process.execPath, args);
        child.stdout.setEncoding('utf8');
        child.stderr.setEncoding('utf8');
        child.stdout.on('data', childPrint);
        child.stderr.on('data', childPrint);

        let output = '';
        function waitForListenHint(text) {
          output += text;
          if (/Debugger listening on ws:\/\/\[?(.+?)\]?:(\d+)\//.test(output)) {
            const host = RegExp.$1;
            const port = Number.parseInt(RegExp.$2);
            child.stderr.removeListener('data', waitForListenHint);
            resolve([child, port, host]);
          }
        }

        child.stderr.on('data', waitForListenHint);
      });
    });
}

function createAgentProxy(domain, client) {
  const agent = new EventEmitter();
  agent.then = (...args) => {
    // TODO: potentially fetch the protocol and pretty-print it here.
    const descriptor = {
      [util.inspect.custom](depth, { stylize }) {
        return stylize(`[Agent ${domain}]`, 'special');
      },
    };
    return Promise.resolve(descriptor).then(...args);
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
      this._runScript = runScript.bind(null,
                                       options.script,
                                       options.scriptArgs,
                                       options.host,
                                       options.port,
                                       this.childPrint.bind(this));
    } else {
      this._runScript =
          () => Promise.resolve([null, options.port, options.host]);
    }

    this.client = new InspectClient();

    this.domainNames = ['Debugger', 'HeapProfiler', 'Profiler', 'Runtime'];
    this.domainNames.forEach((domain) => {
      this[domain] = createAgentProxy(domain, this.client);
    });
    this.handleDebugEvent = (fullName, params) => {
      const [domain, name] = fullName.split('.');
      if (domain in this) {
        this[domain].emit(name, params);
      }
    };
    this.client.on('debugEvent', this.handleDebugEvent);
    const startRepl = createRepl(this);

    // Handle all possible exits
    process.on('exit', () => this.killChild());
    process.once('SIGTERM', process.exit.bind(process, 0));
    process.once('SIGHUP', process.exit.bind(process, 0));

    this.run()
      .then(() => startRepl())
      .then((repl) => {
        this.repl = repl;
        this.repl.on('exit', () => {
          process.exit(0);
        });
        this.paused = false;
      })
      .then(null, (error) => process.nextTick(() => { throw error; }));
  }

  suspendReplWhile(fn) {
    if (this.repl) {
      this.repl.rli.pause();
    }
    this.stdin.pause();
    this.paused = true;
    return new Promise((resolve) => {
      resolve(fn());
    }).then(() => {
      this.paused = false;
      if (this.repl) {
        this.repl.rli.resume();
        this.repl.displayPrompt();
      }
      this.stdin.resume();
    }).then(null, (error) => process.nextTick(() => { throw error; }));
  }

  killChild() {
    this.client.reset();
    if (this.child) {
      this.child.kill();
      this.child = null;
    }
  }

  run() {
    this.killChild();

    return this._runScript().then(([child, port, host]) => {
      this.child = child;

      let connectionAttempts = 0;
      const attemptConnect = () => {
        ++connectionAttempts;
        debuglog('connection attempt #%d', connectionAttempts);
        this.stdout.write('.');
        return this.client.connect(port, host)
          .then(() => {
            debuglog('connection established');
            this.stdout.write(' ok');
          }, (error) => {
            debuglog('connect failed', error);
            // If it's failed to connect 10 times then print failed message
            if (connectionAttempts >= 10) {
              this.stdout.write(' failed to connect, please retry\n');
              process.exit(1);
            }

            return new Promise((resolve) => setTimeout(resolve, 500))
              .then(attemptConnect);
          });
      };

      this.print(`connecting to ${host}:${port} ..`, true);
      return attemptConnect();
    });
  }

  clearLine() {
    if (this.stdout.isTTY) {
      this.stdout.cursorTo(0);
      this.stdout.clearLine(1);
    } else {
      this.stdout.write('\b');
    }
  }

  print(text, oneline = false) {
    this.clearLine();
    this.stdout.write(oneline ? text : `${text}\n`);
  }

  childPrint(text) {
    this.print(
      text.toString()
        .split(/\r\n|\r|\n/g)
        .filter((chunk) => !!chunk)
        .map((chunk) => `< ${chunk}`)
        .join('\n')
    );
    if (!this.paused) {
      this.repl.displayPrompt(true);
    }
    if (/Waiting for the debugger to disconnect\.\.\.\n$/.test(text)) {
      this.killChild();
    }
  }
}

function parseArgv([target, ...args]) {
  let host = '127.0.0.1';
  let port = 9229;
  let isRemote = false;
  let script = target;
  let scriptArgs = args;

  const hostMatch = target.match(/^([^:]+):(\d+)$/);
  const portMatch = target.match(/^--port=(\d+)$/);

  if (hostMatch) {
    // Connecting to remote debugger
    // `node-inspect localhost:9229`
    host = hostMatch[1];
    port = parseInt(hostMatch[2], 10);
    isRemote = true;
    script = null;
  } else if (portMatch) {
    // start debugee on custom port
    // `node inspect --port=9230 script.js`
    port = parseInt(portMatch[1], 10);
    script = args[0];
    scriptArgs = args.slice(1);
  } else if (args.length === 1 && /^\d+$/.test(args[0]) && target === '-p') {
    // Start debugger against a given pid
    const pid = parseInt(args[0], 10);
    try {
      process._debugProcess(pid);
    } catch (e) {
      if (e.code === 'ESRCH') {
        /* eslint-disable no-console */
        console.error(`Target process: ${pid} doesn't exist.`);
        /* eslint-enable no-console */
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

function startInspect(argv = process.argv.slice(2),
                      stdin = process.stdin,
                      stdout = process.stdout) {
  /* eslint-disable no-console */
  if (argv.length < 1) {
    const invokedAs = runAsStandalone ?
      'node-inspect' :
      `${process.argv0} ${process.argv[1]}`;

    console.error(`Usage: ${invokedAs} script.js`);
    console.error(`       ${invokedAs} <host>:<port>`);
    console.error(`       ${invokedAs} -p <pid>`);
    process.exit(1);
  }

  const options = parseArgv(argv);
  const inspector = new NodeInspector(options, stdin, stdout);

  stdin.resume();

  function handleUnexpectedError(e) {
    if (!(e instanceof StartupError)) {
      console.error('There was an internal error in node-inspect. ' +
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
  /* eslint-enable no-console */
}
exports.start = startInspect;
