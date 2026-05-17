'use strict';

const {
  ArrayFrom,
  ArrayIsArray,
  ArrayPrototypeJoin,
  ArrayPrototypeMap,
  ArrayPrototypePush,
  ArrayPrototypeSlice,
  FunctionPrototypeBind,
  JSONStringify,
  NumberIsNaN,
  NumberParseInt,
  ObjectEntries,
  Promise,
  RegExpPrototypeExec,
  RegExpPrototypeSymbolSplit,
  SafeMap,
  SafeSet,
  StringPrototypeIncludes,
  StringPrototypeSlice,
  StringPrototypeStartsWith,
} = primordials;

const { clearTimeout, setTimeout } = require('timers');
const util = require('util');
const { SideEffectFreeRegExpPrototypeSymbolReplace } = require('internal/util');

const InspectClient = require('internal/debugger/inspect_client');
const {
  ensureTrailingNewline,
  launchChildProcess,
  writeUsageAndExit,
} = require('internal/debugger/inspect_helpers');

const { ERR_DEBUGGER_STARTUP_ERROR } = require('internal/errors').codes;
const {
  exitCodes: {
    kGenericUserError,
    kNoFailure,
  },
} = internalBinding('errors');

const kProbeDefaultTimeout = 30000;
const kProbeVersion = 1;
const kProbeDisconnectSentinel = 'Waiting for the debugger to disconnect...';
const kDigitsRegex = /^\d+$/;
const kInspectPortRegex = /^--inspect-port=(\d+)$/;
const kProbeArgOptions = {
  __proto__: null,
  expr: { type: 'string' },
  json: { type: 'boolean' },
  // Port and timeout use type 'string' because parseArgs has no
  // numeric type; the values are parsed to integers in parseProbeArgv().
  port: { type: 'string' },
  preview: { type: 'boolean' },
  probe: { type: 'string' },
  timeout: { type: 'string' },
};

function parseUnsignedInteger(value, name, allowZero = false) {
  if (typeof value !== 'string' || RegExpPrototypeExec(kDigitsRegex, value) === null) {
    throw new ERR_DEBUGGER_STARTUP_ERROR(`Invalid ${name}: ${value}`);
  }
  const parsed = NumberParseInt(value, 10);
  if (NumberIsNaN(parsed) || (!allowZero && parsed < 1)) {
    throw new ERR_DEBUGGER_STARTUP_ERROR(`Invalid ${name}: ${value}`);
  }
  return parsed;
}

// Accepts file:line or file:line:column formats.
// Non-greedy (.+?) allows Windows drive-letter paths like C:\foo.js:10.
function parseProbeLocation(text) {
  const match = RegExpPrototypeExec(/^(.+?):(\d+)(?::(\d+))?$/, text);
  if (match === null) {
    throw new ERR_DEBUGGER_STARTUP_ERROR(`Invalid probe location: ${text}`);
  }

  const file = match[1];
  const line = parseUnsignedInteger(match[2], 'probe location');
  const column = match[3] !== undefined ?
    parseUnsignedInteger(match[3], 'probe location') : undefined;
  const target = column === undefined ? [file, line] : [file, line, column];

  return {
    file,
    lineNumber: line - 1,
    columnNumber: column === undefined ? undefined : column - 1,
    target,
  };
}

function formatPendingProbeLocations(probes, pending) {
  const seen = new SafeSet();
  for (const probeIndex of pending) {
    seen.add(ArrayPrototypeJoin(probes[probeIndex].target, ':'));
  }
  return ArrayPrototypeJoin(ArrayFrom(seen), ', ');
}

function formatTargetExitMessage(probes, pending, exitCode, signal) {
  const status = signal === null ?
    `Target exited with code ${exitCode}` :
    `Target exited with signal ${signal}`;
  if (pending.length === 0) {
    return `${status} before target completion`;
  }
  return `${status} before probes: ${formatPendingProbeLocations(probes, pending)}`;
}

// Trim the "Waiting for the debugger to disconnect..." message from stderr for reporting child errors.
function trimProbeChildStderr(stderr) {
  const lines = RegExpPrototypeSymbolSplit(/\r\n|\r|\n/g, stderr);
  const kept = [];
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];
    if (line === '' && i === lines.length - 1) { continue; }
    if (line === kProbeDisconnectSentinel) { continue; }
    ArrayPrototypePush(kept, line);
  }
  return ArrayPrototypeJoin(kept, '\n');
}

function formatPreviewPropertyValue(property) {
  if (property.type === 'string') {
    return JSONStringify(property.value ?? '');
  }
  return property.value ?? property.type;
}

function trimRemoteObject(result) {
  if (result === undefined || result === null || typeof result !== 'object') {
    return result;
  }

  if (ArrayIsArray(result)) {
    return ArrayPrototypeMap(result, trimRemoteObject);
  }

  const trimmed = { __proto__: null };
  for (const { 0: key, 1: value } of ObjectEntries(result)) {
    if (key === 'objectId' || key === 'className') {
      continue;
    }
    trimmed[key] = trimRemoteObject(value);
  }
  return trimmed;
}

function stripProbePreviews(value) {
  if (value === undefined || value === null || typeof value !== 'object') {
    return value;
  }

  if (ArrayIsArray(value)) {
    return ArrayPrototypeMap(value, stripProbePreviews);
  }

  const stripped = { __proto__: null };
  for (const { 0: key, 1: entry } of ObjectEntries(value)) {
    if (key === 'preview') {
      continue;
    }
    stripped[key] = stripProbePreviews(entry);
  }
  return stripped;
}

// Format CDP RemoteObject values into more readable formats.
function formatRemoteObject(result) {
  if (result === undefined) { return 'undefined'; }

  switch (result.type) {
    case 'undefined':
      return 'undefined';
    case 'string':
      return JSONStringify(result.value);
    case 'number':
      if (result.unserializableValue !== undefined) {
        return result.unserializableValue;
      }
      return `${result.value}`;
    case 'boolean':
      return `${result.value}`;
    case 'symbol':
      return result.description || 'Symbol()';
    case 'bigint':
      return result.unserializableValue ?? result.description ?? '0n';
    case 'function':
      return result.description || 'function()';
    case 'object':
      if (result.subtype === 'null') {
        return 'null';
      }
      if (result.subtype === 'error') {
        return result.description || 'Error';
      }
      if (result.preview !== undefined) {
        const properties = ArrayPrototypeJoin(ArrayPrototypeMap(
          result.preview.properties,
          result.preview.subtype === 'array' ?
            (property) => formatPreviewPropertyValue(property) :
            (property) => `${property.name}: ${formatPreviewPropertyValue(property)}`,
        ), ', ');
        const suffix = result.preview.overflow ? ', ...' : '';
        if (result.preview.subtype === 'array') {
          return `[${properties}${suffix}]`;
        }
        return `{${properties}${suffix}}`;
      }
      return result.description || result.className || 'Object';
    default:
      return `${result.value ?? result.description ?? ''}`;
  }
}

// Built human-readable text output for probe reports.
function buildProbeTextReport(report) {
  const lines = [];

  for (const result of report.results) {
    if (result.event === 'hit') {
      const probe = report.probes[result.probe];
      const location = ArrayPrototypeJoin(probe.target, ':');
      ArrayPrototypePush(lines, `Hit ${result.hit} at ${location}`);
      if (result.error !== undefined) {
        ArrayPrototypePush(lines,
                           `  [error] ${probe.expr} = ` +
                           `${formatRemoteObject(result.error)}`);
      } else {
        ArrayPrototypePush(lines,
                           `  ${probe.expr} = ` +
                           `${formatRemoteObject(result.result)}`);
      }
      continue;
    }

    if (result.event === 'completed') {
      ArrayPrototypePush(lines, 'Completed');
      continue;
    }

    if (result.event === 'miss') {
      ArrayPrototypePush(lines,
                         `Missed probes: ` +
                         `${formatPendingProbeLocations(report.probes, result.pending)}`);
      continue;
    }

    if (result.event === 'timeout') {
      ArrayPrototypePush(lines, result.error.message);
      continue;
    }

    if (result.event === 'error') {
      ArrayPrototypePush(lines, result.error.message);
      if (result.error.stderr !== undefined) {
        ArrayPrototypePush(lines, '  [stderr]');
        const stderrLines = RegExpPrototypeSymbolSplit(
          /\r\n|\r|\n/g,
          result.error.stderr,
        );
        for (let i = 0; i < stderrLines.length; i++) {
          if (stderrLines[i] === '' && i === stderrLines.length - 1) {
            continue;
          }
          ArrayPrototypePush(lines, `  ${stderrLines[i]}`);
        }
      }
    }
  }

  return ensureTrailingNewline(ArrayPrototypeJoin(lines, '\n'));
}

function hasTopLevelProbeOption(args) {
  const { tokens } = util.parseArgs({
    args,
    allowPositionals: true,
    options: kProbeArgOptions,
    strict: false,
    tokens: true,
  });

  for (const token of tokens) {
    if (token.kind === 'option' && token.name === 'probe') {
      return true;
    }

    if (token.kind === 'option-terminator' || token.kind === 'positional') {
      return false;
    }
  }

  return false;
}

function parseProbeArgv(args) {
  let port = 0;
  let preview = false;
  let timeout = kProbeDefaultTimeout;
  let json = false;
  let sawSeparator = false;
  let childStartIndex = args.length;
  let pendingLocation;
  let expectedExprIndex = -1;
  const probes = [];

  const { tokens } = util.parseArgs({
    args,
    allowPositionals: true,
    options: kProbeArgOptions,
    strict: false,
    tokens: true,
  });

  for (const token of tokens) {
    if (token.kind === 'option-terminator') {
      sawSeparator = true;
      childStartIndex = token.index + 1;
      break;
    }

    if (pendingLocation !== undefined) {
      if (token.kind === 'option' &&
          token.name === 'expr' &&
          token.index === expectedExprIndex &&
          token.value !== undefined) {
        ArrayPrototypePush(probes, {
          expr: token.value,
          location: pendingLocation,
        });
        pendingLocation = undefined;
        continue;
      }

      throw new ERR_DEBUGGER_STARTUP_ERROR(
        'Each --probe must be followed immediately by --expr <expr>');
    }

    if (token.kind === 'positional') {
      childStartIndex = token.index;
      break;
    }

    switch (token.name) {
      case 'json':
        json = true;
        break;
      case 'timeout':
        if (token.value === undefined) {
          throw new ERR_DEBUGGER_STARTUP_ERROR(`Missing value for ${token.rawName}`);
        }
        timeout = parseUnsignedInteger(token.value, 'timeout', true);
        break;
      case 'port':
        if (token.value === undefined) {
          throw new ERR_DEBUGGER_STARTUP_ERROR(`Missing value for ${token.rawName}`);
        }
        port = parseUnsignedInteger(token.value, 'inspector port', true);
        break;
      case 'preview':
        preview = true;
        break;
      case 'probe':
        pendingLocation = parseProbeLocation(token.value);
        expectedExprIndex = token.index + (token.inlineValue ? 1 : 2);
        break;
      case 'expr':
        throw new ERR_DEBUGGER_STARTUP_ERROR('Unexpected --expr before --probe');
      default:
        if (probes.length > 0) {
          throw new ERR_DEBUGGER_STARTUP_ERROR(
            'Use -- before child Node.js flags in probe mode');
        }
        throw new ERR_DEBUGGER_STARTUP_ERROR(`Unknown probe option: ${token.rawName}`);
    }
  }

  if (pendingLocation !== undefined) {
    throw new ERR_DEBUGGER_STARTUP_ERROR(
      'Each --probe must be followed immediately by --expr <expr>');
  }

  if (probes.length === 0) {
    throw new ERR_DEBUGGER_STARTUP_ERROR(
      'Probe mode requires at least one --probe <loc> --expr <expr> group');
  }

  const childArgv = ArrayPrototypeSlice(args, childStartIndex);
  if (childArgv.length === 0) {
    throw new ERR_DEBUGGER_STARTUP_ERROR('Probe mode requires a child script');
  }

  if (!sawSeparator && StringPrototypeStartsWith(childArgv[0], '-')) {
    throw new ERR_DEBUGGER_STARTUP_ERROR('Use -- before child Node.js flags in probe mode');
  }

  let skipPortPreflight = port === 0;
  for (const arg of childArgv) {
    const inspectPortMatch = RegExpPrototypeExec(kInspectPortRegex, arg);
    if (inspectPortMatch === null) {
      continue;
    }
    if (inspectPortMatch[1] === '0') {
      skipPortPreflight = true;
      continue;
    }
    throw new ERR_DEBUGGER_STARTUP_ERROR(
      'Only child --inspect-port=0 is supported in probe mode');
  }

  return {
    host: '127.0.0.1',
    port,
    preview,
    timeout,
    json,
    probes,
    childArgv,
    skipPortPreflight,
  };
}

class ProbeInspectorSession {
  constructor(options) {
    this.options = options;
    this.client = new InspectClient();
    this.child = null;
    this.cleanupStarted = false;
    this.childStderr = '';
    this.disconnectRequested = false;
    this.finished = false;
    this.started = false;
    this.stderrBuffer = '';
    this.breakpointDefinitions = new SafeMap();
    this.results = [];
    this.timeout = null;
    this.resolveCompletion = null;
    this.completionPromise = new Promise((resolve) => {
      this.resolveCompletion = resolve;
    });
    this.probes = ArrayPrototypeMap(options.probes, (probe) => ({
      expr: probe.expr,
      target: probe.location.target,
      location: probe.location,
      hits: 0,
    }));

    this.onChildOutput = FunctionPrototypeBind(this.onChildOutput, this);
    this.onChildExit = FunctionPrototypeBind(this.onChildExit, this);
    this.onClientClose = FunctionPrototypeBind(this.onClientClose, this);
    this.onPaused = FunctionPrototypeBind(this.onPaused, this);
  }

  finish(state) {
    if (this.finished) { return; }
    this.finished = true;
    if (this.timeout !== null) {
      clearTimeout(this.timeout);
      this.timeout = null;
    }
    this.resolveCompletion(state);
  }

  onChildOutput(text, which) {
    if (which !== 'stderr') { return; }

    if (this.started) {
      this.childStderr += text;
    }

    const combined = this.stderrBuffer + text;
    if (this.started &&
        StringPrototypeIncludes(combined, kProbeDisconnectSentinel)) {
      this.disconnectRequested = true;
      this.client.reset();
    }

    if (combined.length > kProbeDisconnectSentinel.length) {
      this.stderrBuffer = StringPrototypeSlice(combined,
                                               combined.length -
                                               kProbeDisconnectSentinel.length);
    } else {
      this.stderrBuffer = combined;
    }
  }

  onChildExit(code, signal) {
    if (this.started) {
      if (code !== 0 || signal !== null) {
        this.finish({
          __proto__: null,
          event: 'error',
          exitCode: code,
          signal,
          stderr: trimProbeChildStderr(this.childStderr),
        });
      } else {
        this.finish('complete');
      }
    }
  }

  onClientClose() {
    if (!this.started || this.child === null) { return; }

    // TODO(joyeecheung): Surface mid-probe inspector disconnects as terminal probe errors
    // instead of deferring to timeout or miss classification.
    if (this.disconnectRequested) { return; }

    if (this.child.exitCode !== null || this.child.signalCode !== null) {
      this.onChildExit(this.child.exitCode, this.child.signalCode);
    }
  }

  onPaused(params) {
    // TODO(joyeecheung): Preserve evaluation and resume failures as terminal probe errors
    // instead of collapsing them into a synthetic completion.
    this.handlePaused(params).catch((error) => {
      if (!this.finished) {
        if (error?.code === 'ERR_DEBUGGER_ERROR') {
          if (this.child !== null &&
              (this.child.exitCode !== null || this.child.signalCode !== null)) {
            this.onChildExit(this.child.exitCode, this.child.signalCode);
          }
          return;
        }
        this.finish('complete');
      }
    });
  }

  async handlePaused(params) {
    if (this.finished) { return; }

    const hitBreakpoints = params.hitBreakpoints;
    if (hitBreakpoints === undefined || hitBreakpoints.length === 0) {
      await this.resume();
      return;
    }

    const callFrameId = params.callFrames?.[0]?.callFrameId;
    if (callFrameId === undefined) {
      await this.resume();
      return;
    }

    for (const breakpointId of hitBreakpoints) {
      const definition = this.breakpointDefinitions.get(breakpointId);
      if (definition === undefined) { continue; }
      for (const probeIndex of definition.probeIndices) {
        await this.evaluateProbe(callFrameId, probeIndex);
      }
    }

    await this.resume();
  }

  async evaluateProbe(callFrameId, probeIndex) {
    const probe = this.probes[probeIndex];
    const evaluation = await this.client.callMethod('Debugger.evaluateOnCallFrame', {
      callFrameId,
      expression: probe.expr,
      generatePreview: true,
    });

    probe.hits++;
    const result = { probe: probeIndex, event: 'hit', hit: probe.hits };

    if (evaluation.exceptionDetails !== undefined) {
      result.error = evaluation.result === undefined ? {
        type: 'object',
        subtype: 'error',
        description: 'Probe expression failed',
      } : trimRemoteObject(evaluation.result);
    } else {
      result.result = trimRemoteObject(evaluation.result);
    }

    ArrayPrototypePush(this.results, result);
  }

  async resume() {
    if (this.finished) { return; }
    await this.client.callMethod('Debugger.resume');
  }

  startTimeout() {
    this.timeout = setTimeout(() => { this.finish('timeout'); }, this.options.timeout);
    this.timeout.unref();
  }

  attachListeners() {
    this.child.on('exit', this.onChildExit);
    this.client.on('close', this.onClientClose);
    this.client.on('Debugger.paused', this.onPaused);
  }

  async bindBreakpoints() {
    const uniqueLocations = new SafeMap();

    for (let probeIndex = 0; probeIndex < this.probes.length; probeIndex++) {
      const probe = this.probes[probeIndex];
      const key = `${probe.location.file}\n${probe.location.lineNumber}\n` +
                  `${probe.location.columnNumber === undefined ? '' : probe.location.columnNumber}`;
      let entry = uniqueLocations.get(key);
      if (entry === undefined) {
        entry = { location: probe.location, probeIndices: [] };
        uniqueLocations.set(key, entry);
      }
      ArrayPrototypePush(entry.probeIndices, probeIndex);
    }

    for (const { location, probeIndices } of uniqueLocations.values()) {
      // TODO(joyeecheung): Normalize relative probe paths and avoid suffix matches that can
      // bind unrelated loaded scripts with the same basename.
      // On Windows, normalize backslashes to forward slashes so the regex matches
      // V8 script URLs which always use forward slashes.
      const normalizedFile = process.platform === 'win32' ?
        SideEffectFreeRegExpPrototypeSymbolReplace(/\\/g, location.file, '/') :
        location.file;
      const escapedPath = SideEffectFreeRegExpPrototypeSymbolReplace(
        /([/\\.?*()^${}|[\]])/g,
        normalizedFile,
        '\\$1',
      );
      const params = {
        urlRegex: `^(.*[\\/\\\\])?${escapedPath}$`,
        lineNumber: location.lineNumber,
      };
      if (location.columnNumber !== undefined) {
        params.columnNumber = location.columnNumber;
      }

      const result = await this.client.callMethod('Debugger.setBreakpointByUrl', params);
      this.breakpointDefinitions.set(result.breakpointId, { probeIndices });
    }
  }

  getPendingProbeIndices() {
    const pending = [];
    for (let probeIndex = 0; probeIndex < this.probes.length; probeIndex++) {
      if (this.probes[probeIndex].hits === 0) {
        ArrayPrototypePush(pending, probeIndex);
      }
    }
    return pending;
  }

  buildReport(state) {
    const pending = this.getPendingProbeIndices();
    const report = {
      v: kProbeVersion,
      probes: ArrayPrototypeMap(this.probes, (probe) => {
        return { expr: probe.expr, target: probe.target };
      }),
      results: ArrayPrototypeSlice(this.results),
    };

    if (state === 'timeout') {
      ArrayPrototypePush(report.results, {
        event: 'timeout',
        pending,
        error: {
          code: 'probe_timeout',
          message: pending.length === 0 ?
            `Timed out after ${this.options.timeout}ms waiting for target completion` :
            `Timed out after ${this.options.timeout}ms waiting for probes: ` +
            `${formatPendingProbeLocations(this.probes, pending)}`,
        },
      });
      return { code: kGenericUserError, report };
    }

    if (state?.event === 'error') {
      const error = {
        __proto__: null,
        code: 'probe_target_exit',
        message: formatTargetExitMessage(this.probes, pending, state.exitCode, state.signal),
      };
      if (state.exitCode !== null) {
        error.exitCode = state.exitCode;
      }
      if (state.signal !== null) {
        error.signal = state.signal;
      }
      error.stderr = state.stderr;
      ArrayPrototypePush(report.results, { event: 'error', pending, error });
      return { code: kNoFailure, report };
    }

    if (pending.length === 0) {
      ArrayPrototypePush(report.results, { event: 'completed' });
    } else {
      ArrayPrototypePush(report.results, { event: 'miss', pending });
    }

    return { code: kNoFailure, report };
  }

  async cleanup() {
    if (this.cleanupStarted) { return; }
    this.cleanupStarted = true;

    if (this.timeout !== null) {
      clearTimeout(this.timeout);
      this.timeout = null;
    }

    this.client.reset();

    if (this.child === null) { return; }

    if (this.child.exitCode === null && this.child.signalCode === null) {
      this.child.kill();
    }
  }

  async run() {
    try {
      const { childArgv, host, port, skipPortPreflight } = this.options;
      const { 0: child, 1: actualPort, 2: actualHost } =
        await launchChildProcess(childArgv,
                                 host,
                                 port,
                                 this.onChildOutput,
                                 { skipPortPreflight });
      this.child = child;
      this.attachListeners();

      await this.client.connect(actualPort, actualHost);
      await this.client.callMethod('Runtime.enable');
      await this.client.callMethod('Debugger.enable');
      await this.bindBreakpoints();
      this.started = true;
      this.startTimeout();

      await this.client.callMethod('Runtime.runIfWaitingForDebugger');

      const state = await this.completionPromise;
      return this.buildReport(state);
    } finally {
      await this.cleanup();
    }
  }
}

async function runProbeMode(stdout, probeOptions) {
  try {
    const session = new ProbeInspectorSession(probeOptions);
    const { code, report } = await session.run();
    stdout.write(probeOptions.json ?
      `${JSONStringify(probeOptions.preview ? report : stripProbePreviews(report))}\n` :
      buildProbeTextReport(report));
    process.exit(code);
  } catch (error) {
    if (error.childStderr) {
      process.stderr.write(error.childStderr);
    }
    process.stderr.write(ensureTrailingNewline(error.message));
    process.exit(kGenericUserError);
  }
}

function startProbeMode(invokedAs, args, stdout) {
  if (!hasTopLevelProbeOption(args)) {
    return false;
  }

  let probeOptions;
  try {
    probeOptions = parseProbeArgv(args);
  } catch (error) {
    writeUsageAndExit(invokedAs, error.message, kGenericUserError);
  }

  runProbeMode(stdout, probeOptions);
  return true;
}

module.exports = {
  startProbeMode,
};
