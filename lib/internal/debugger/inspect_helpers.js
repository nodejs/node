'use strict';

const {
  ArrayPrototypePushApply,
  Number,
  Promise,
  RegExpPrototypeExec,
  StringPrototypeEndsWith,
} = primordials;

const { spawn } = require('child_process');
const net = require('net');
const {
  setInterval: pSetInterval,
  setTimeout: pSetTimeout,
} = require('timers/promises');
const {
  AbortController,
} = require('internal/abort_controller');

const { ERR_DEBUGGER_STARTUP_ERROR } = require('internal/errors').codes;
const {
  exitCodes: {
    kInvalidCommandLineArgument,
  },
} = internalBinding('errors');

const debugRegex = /Debugger listening on ws:\/\/\[?(.+?)\]?:(\d+)\//;

async function portIsFree(host, port, timeout = 3000) {
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
      socket.on('connect', () => {
        socket.end();
        resolve();
      });
    });
    if (error?.code === 'ECONNREFUSED') {
      return;
    }
  }
}

function ensureTrailingNewline(text) {
  return StringPrototypeEndsWith(text, '\n') ? text : `${text}\n`;
}

function writeInspectUsageAndExit(invokedAs, message, exitCode) {
  const code = exitCode ?? (message ? kInvalidCommandLineArgument : 0);
  const out = code === 0 ? process.stdout : process.stderr;
  if (message) {
    out.write(`${message}\n`);
  }
  out.write(`Usage: ${invokedAs} [--port=<port>] [<node-option> ...]
                      [<script> [<script-args>] | <host>:<port> | -p <pid>]
       ${invokedAs} --probe <file>:<line>[:<col>] --expr <expr>
                      [--probe <file>:<line>[:<col>] --expr <expr> ...]
                      [--json] [--preview] [--timeout=<ms>] [--port=<port>]
                      [--] [<node-option> ...] <script> [<script-args> ...]

Interactive mode: Starts a live debugging session.

Example:
  $ node inspect script.js

Options:
  --port=<port>         Inspector port for the debuggee (default: 9229)
  <script>              The script to launch and debug.
  <host>:<port>         Remote debugger to connect to.
  -p <pid>              Attach to a running Node.js process by PID

Semantics:
* If neither a script nor a host:port nor -p is provided, node inspect starts
  the REPL.

Non-interactive probe mode: Evaluates expressions whenever execution reaches
specified source locations and prints all the evaluation results to stdout.

Example:
  $ node inspect --probe app.js:10 --expr "user"
                 --probe src/utils.js:5:15 --expr "config.options"
                 --json --preview -- --no-warnings app.js --arg-for-app=foo

Options:
  --probe <file>:<line>[:<col>]
                    Source location of the probe (1-based, col defaults
                    to 1). Matches by file basename, use a fuller path to
                    disambiguate. Must be immediately followed by --expr.
  --expr <expr>     Expression to evaluate in the lexical scope of the
                    preceding --probe each time execution reaches it.
                    Avoid probing let/const-bound variables at their
                    declaration site or a ReferenceError may be thrown.
  --json            Output JSON if specified, otherwise human-readable text.
  --preview         Include V8 object previews in JSON output.
  --timeout <ms>    Global session timeout (default: 30000).
  --port <port>     Inspector port for the debuggee (default: 0 = random).

Semantics:
* Multiple --probe/--expr pairs are allowed. Same-location --probes share
  a pause and scope, their --exprs are evaluated in command-line order.
* Use -- before any Node.js flags intended for the child process.
* Target errors are surfaced in the report as a terminal 'error' event.
  The probing process exits 0 unless it encounters an error itself.

See https://nodejs.org/api/debugger.html for details, including the
probe output schema.
`);
  process.exit(code);
}

async function launchChildProcess(childArgs, inspectHost, inspectPort,
                                  childOutput, options = { __proto__: null }) {
  if (!options.skipPortPreflight) {
    await portIsFree(inspectHost, inspectPort);
  }

  const args = [`--inspect-brk=${inspectPort}`];
  ArrayPrototypePushApply(args, childArgs);

  const child = spawn(process.execPath, args);
  child.stdout.setEncoding('utf8');
  child.stderr.setEncoding('utf8');
  child.stdout.on('data', (chunk) => childOutput(chunk, 'stdout'));
  child.stderr.on('data', (chunk) => childOutput(chunk, 'stderr'));

  let stderrOutput = '';
  return new Promise((resolve, reject) => {
    function rejectLaunch(message) {
      reject(new ERR_DEBUGGER_STARTUP_ERROR(message, { childStderr: stderrOutput }));
    }

    function onExit(code, signal) {
      const suffix = signal !== null ? ` (${signal})` : ` (code ${code})`;
      rejectLaunch(`Target exited before the inspector was ready${suffix}`);
    }

    function onError(error) {
      rejectLaunch(error.message);
    }

    function onStderr(text) {
      stderrOutput += text;
      const debug = RegExpPrototypeExec(debugRegex, stderrOutput);
      if (debug) {
        child.stderr.removeListener('data', onStderr);
        child.removeListener('exit', onExit);
        child.removeListener('error', onError);
        resolve([child, Number(debug[2]), debug[1]]);
      }
    }

    child.once('exit', onExit);
    child.once('error', onError);
    child.stderr.on('data', onStderr);
  });
}

module.exports = {
  ensureTrailingNewline,
  launchChildProcess,
  writeInspectUsageAndExit,
};
