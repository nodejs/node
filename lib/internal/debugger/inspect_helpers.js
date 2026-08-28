'use strict';

const {
  ArrayPrototypePop,
  ArrayPrototypePush,
  ArrayPrototypePushApply,
  MapPrototypeGet,
  Number,
  Promise,
  PromiseWithResolvers,
  RegExpPrototypeExec,
  RegExpPrototypeSymbolReplace,
  SafePromiseRace,
  StringPrototypeEndsWith,
  StringPrototypeIndexOf,
  StringPrototypeSlice,
  StringPrototypeStartsWith,
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

const {
  ERR_DEBUGGER_ERROR,
  ERR_DEBUGGER_STARTUP_ERROR,
} = require('internal/errors').codes;
const {
  exitCodes: {
    kInvalidCommandLineArgument,
  },
} = internalBinding('errors');
const {
  types: {
    kBoolean,
    kNoOp,
    kV8Option,
  },
} = internalBinding('options');

const { getCLIOptionsInfo } = require('internal/options');

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

// Mirror OptionsParser::Parse() far enough to find the child script. Options
// before it must not undo the inspector setup added by launchChildProcess().
function validateChildArgs(childArgs) {
  const { options, aliases } = getCLIOptionsInfo();
  const syntheticArgs = [];
  let breakFirstLine = true;
  let childArgIndex = 0;
  let inspectorEnabled = true;

  function peekArg() {
    return syntheticArgs.length > 0 ?
      syntheticArgs[syntheticArgs.length - 1] :
      childArgs[childArgIndex];
  }

  function shiftArg() {
    return syntheticArgs.length > 0 ?
      ArrayPrototypePop(syntheticArgs) :
      childArgs[childArgIndex++];
  }

  while (true) {
    const nextArg = peekArg();
    if (nextArg === undefined || nextArg.length <= 1 || nextArg[0] !== '-') {
      break;
    }

    const isSynthetic = syntheticArgs.length > 0;
    const arg = shiftArg();
    if (arg === '--') { break; }
    if (!isSynthetic &&
        (arg === '--experimental-config-file' ||
         arg === '--experimental-default-config-file')) {
      // ConfigReader rewrites these to an inline default path before parsing.
      continue;
    }
    if (!isSynthetic &&
        StringPrototypeStartsWith(
          arg, '--experimental-default-config-file=')) {
      // ConfigReader rejects this form before parsing the remaining options.
      return;
    }

    const equalsIndex = arg[1] === '-' ? StringPrototypeIndexOf(arg, '=') : -1;
    let name = equalsIndex === -1 ? arg : StringPrototypeSlice(arg, 0, equalsIndex);
    if (name.length > 2) {
      name = `${StringPrototypeSlice(name, 0, 2)}${
        RegExpPrototypeSymbolReplace(/_/g, StringPrototypeSlice(name, 2), '-')}`;
    }

    let isNegation = false;
    if (StringPrototypeStartsWith(name, '--no-')) {
      name = `--${StringPrototypeSlice(name, 5)}`;
      isNegation = true;
    }

    while (true) {
      let expansion = MapPrototypeGet(aliases, name);
      if (expansion === undefined && equalsIndex !== -1) {
        expansion = MapPrototypeGet(aliases, `${name}=`);
      }
      const aliasArg = peekArg();
      if (expansion === undefined &&
          aliasArg !== undefined &&
          aliasArg.length > 0 &&
          aliasArg[0] !== '-') {
        expansion = MapPrototypeGet(aliases, `${name} <arg>`);
      }
      if (expansion === undefined) { break; }

      const previousName = name;
      // process.allowedNodeEnvironmentFlags may remove a self-recursive
      // first entry from the cached alias metadata. Preserve the native
      // parser's synthetic option terminator in that case.
      if (expansion[0] === '--') {
        for (let i = expansion.length - 1; i >= 0; i--) {
          ArrayPrototypePush(syntheticArgs, expansion[i]);
        }
        break;
      }
      name = expansion[0];
      for (let i = expansion.length - 1; i > 0; i--) {
        ArrayPrototypePush(syntheticArgs, expansion[i]);
      }
      if (name === previousName) { break; }
    }

    const info = MapPrototypeGet(options, name);
    if (info === undefined) { continue; }
    if (isNegation && info.type !== kBoolean && info.type !== kV8Option) {
      return;
    }
    if (info.type === kBoolean || info.type === kNoOp || info.type === kV8Option) {
      if (name === '--inspect') {
        inspectorEnabled = !isNegation;
      } else if (name === '--inspect-brk') {
        breakFirstLine = !isNegation;
        if (!isNegation) { inspectorEnabled = true; }
      } else if (!isNegation &&
                 (name === '--inspect-wait' ||
                  name === '--inspect-brk-node')) {
        inspectorEnabled = true;
      }
      continue;
    }

    if (equalsIndex !== -1) {
      if (equalsIndex === arg.length - 1) { return; }
      continue;
    }

    const value = peekArg();
    if (value === undefined || (value.length > 0 && value[0] === '-')) {
      return;
    }
    shiftArg();
  }

  if (!inspectorEnabled) {
    throw new ERR_DEBUGGER_STARTUP_ERROR(
      '--no-inspect is incompatible with node inspect before the child script');
  }
  if (!breakFirstLine) {
    throw new ERR_DEBUGGER_STARTUP_ERROR(
      '--no-inspect-brk is incompatible with node inspect before the child script');
  }
}

async function waitForDebugger(
  client,
  callMethod = (method) => client.callMethod(method),
) {
  const {
    promise: waitingPromise,
    resolve: resolveWaiting,
  } = PromiseWithResolvers();
  const {
    promise: closedPromise,
    reject: rejectClosed,
  } = PromiseWithResolvers();
  const onWaiting = () => resolveWaiting();
  const onClose = () => {
    rejectClosed(new ERR_DEBUGGER_ERROR(
      'Debugger session ended while waiting for target startup'));
  };

  // The inspector can accept a connection before the target reaches its
  // startup wait. Enabling NodeRuntime makes that state observable whether
  // the target was already waiting or starts waiting later.
  client.once('NodeRuntime.waitingForDebugger', onWaiting);
  client.once('close', onClose);
  try {
    await SafePromiseRace([
      callMethod('NodeRuntime.enable'),
      closedPromise,
    ]);
    await SafePromiseRace([
      waitingPromise,
      closedPromise,
    ]);
    await SafePromiseRace([
      callMethod('NodeRuntime.disable'),
      closedPromise,
    ]);
  } finally {
    client.removeListener('NodeRuntime.waitingForDebugger', onWaiting);
    client.removeListener('close', onClose);
  }
}

function writeInspectUsageAndExit(invokedAs, message, exitCode) {
  const code = exitCode ?? (message ? kInvalidCommandLineArgument : 0);
  const out = code === 0 ? process.stdout : process.stderr;
  if (message) {
    out.write(`${message}\n`);
  }
  out.write(`Usage: ${invokedAs} [--port=<port>] [<node-option> ...]
                      [<script> [<script-args>] | <host>:<port> | -p <pid>]
       ${invokedAs} --probe <file>:<line>[:<col>] --expr <expr> [--cond <expr>] [--max-hit <n>]
                      [--probe <file>:<line>[:<col>] --expr <expr> [--cond <expr>] [--max-hit <n>] ...]
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
                    Source location of the probe. <file> is matched as a
                    path suffix of every loaded script URL, anchored on
                    a path separator. <line> and the optional <col> are
                    1-based. If <col> is omitted, the probe binds to
                    the first executable column on the line. This option
                    must be immediately followed by a pairing --expr.
  --expr <expr>     Expression to evaluate in the lexical scope of the
                    preceding --probe each time execution reaches it.
                    Avoid probing let/const-bound variables at their
                    declaration site or a ReferenceError may be thrown.
  --cond <expr>     Optional condition for the probe location. The probe only
                    records a hit when <expr> is truthy at the location. A
                    condition that throws is treated as false.
  --max-hit <n>     Per-probe limit on evaluated hits. When not specified,
                    there's no hit limit. When any probe reaches its hit LIMIT,
                    the probing process will detach and report the results.
                    The probed process will continue to run.
  --json            Output JSON if specified, otherwise human-readable text.
  --preview         Include V8 object previews in JSON output.
  --timeout <ms>    Global session timeout (default: 30000).
  --port <port>     Inspector port for the debuggee (default: 0 = random).

Semantics:
* Multiple --probe/--expr pairs are allowed. Same-location --probes share
  a pause and scope, their --exprs are evaluated in command-line order.
* --max-hit scopes to one --probe/--expr pair, so same-location pairs may set
  different limits. --cond scopes to the location, probes sharing a location
  must all share one condition (or none).
* --probe utils.js:<line>[:<col>] matches every loaded utils.js. Pass a
  fuller path e.g. src/utils.js to narrow the match.
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
  validateChildArgs(childArgs);

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
  waitForDebugger,
  writeInspectUsageAndExit,
};
