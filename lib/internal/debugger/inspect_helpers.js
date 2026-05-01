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

function writeUsageAndExit(invokedAs, message, exitCode = kInvalidCommandLineArgument) {
  if (message) {
    process.stderr.write(`${message}\n`);
  }
  const text =
    `Usage: ${invokedAs} script.js\n` +
    `       ${invokedAs} <host>:<port>\n` +
    `       ${invokedAs} --port=<port> Use 0 for random port assignment\n` +
    `       ${invokedAs} -p <pid>\n` +
    `       ${invokedAs} [--json] [--timeout=<ms>] [--port=<port>] ` +
    `--probe <file>:<line>[:<col>] --expr <expr> ` +
    `[--probe <file>:<line>[:<col>] --expr <expr> ...] ` +
    `[--] [<node-option> ...] <script.js> [args...]\n`;
  process.stderr.write(text);
  process.exit(exitCode);
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
  writeUsageAndExit,
};
