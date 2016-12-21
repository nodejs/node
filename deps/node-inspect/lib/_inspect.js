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
const util = require('util');

const [ InspectClient, createRepl ] =
  (typeof __dirname !== 'undefined') ?
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

exports.port = 9229;

function runScript(script, scriptArgs, inspectPort, childPrint) {
  return new Promise((resolve) => {
    const args = [
      '--inspect',
      `--debug-brk=${inspectPort}`,
    ].concat([script], scriptArgs);
    const child = spawn(process.execPath, args);
    child.stdout.setEncoding('utf8');
    child.stderr.setEncoding('utf8');
    child.stdout.on('data', childPrint);
    child.stderr.on('data', childPrint);

    let output = '';
    function waitForListenHint(text) {
      output += text;
      if (/chrome-devtools:\/\//.test(output)) {
        child.stderr.removeListener('data', waitForListenHint);
        resolve(child);
      }
    }

    child.stderr.on('data', waitForListenHint);
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
                                       options.port,
                                       this.childPrint.bind(this));
    } else {
      this._runScript = () => Promise.resolve(null);
    }

    this.client = new InspectClient(options.port, options.host);

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
      .then(() => {
        this.repl = startRepl();
        this.repl.on('exit', () => {
          process.exit(0);
        });
        this.paused = false;
      })
      .then(null, (error) => process.nextTick(() => { throw error; }));
  }

  suspendReplWhile(fn) {
    this.repl.rli.pause();
    this.stdin.pause();
    this.paused = true;
    return new Promise((resolve) => {
      resolve(fn());
    }).then(() => {
      this.paused = false;
      this.repl.rli.resume();
      this.repl.displayPrompt();
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
    return this._runScript().then((child) => {
      this.child = child;

      let connectionAttempts = 0;
      const attemptConnect = () => {
        ++connectionAttempts;
        debuglog('connection attempt #%d', connectionAttempts);
        this.stdout.write('.');
        return this.client.connect()
          .then(() => {
            debuglog('connection established');
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

      const { host, port } = this.options;
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
  let port = exports.port;
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
    // Start debugger on custom port
    // `node debug --port=8058 app.js`
    port = parseInt(portMatch[1], 10);
    script = args[0];
    scriptArgs = args.slice(1);
  }

  return {
    host, port,
    isRemote, script, scriptArgs,
  };
}

function startInspect(argv = process.argv.slice(2),
                      stdin = process.stdin,
                      stdout = process.stdout) {
  /* eslint-disable no-console */
  if (argv.length < 1) {
    console.error('Usage: node-inspect script.js');
    console.error('       node-inspect <host>:<port>');
    process.exit(1);
  }

  const options = parseArgv(argv);
  const inspector = new NodeInspector(options, stdin, stdout);

  stdin.resume();

  function handleUnexpectedError(e) {
    console.error('There was an internal error in node-inspect. ' +
                  'Please report this bug.');
    console.error(e.message);
    console.error(e.stack);
    if (inspector.child) inspector.child.kill();
    process.exit(1);
  }

  process.on('uncaughtException', handleUnexpectedError);
  /* eslint-enable no-console */
}
exports.start = startInspect;
