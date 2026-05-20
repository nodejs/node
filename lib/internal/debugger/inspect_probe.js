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
const { SideEffectFreeRegExpPrototypeSymbolReplace } = require('internal/util');

const InspectClient = require('internal/debugger/inspect_client');
const {
  ensureTrailingNewline,
  launchChildProcess,
} = require('internal/debugger/inspect_helpers');

const { ERR_DEBUGGER_STARTUP_ERROR } = require('internal/errors').codes;
const {
  exitCodes: {
    kGenericUserError,
    kNoFailure,
  },
} = internalBinding('errors');

const kProbeDefaultTimeout = 30000;
const kProbeVersion = 2;
const kProbeDisconnectSentinel = 'Waiting for the debugger to disconnect...';
const kDigitsRegex = /^\d+$/;
const kInspectPortRegex = /^--inspect-port=(\d+)$/;

/**
 * The probe request specified by --probe, serialized into the public report.
 * @typedef {object} ProbeTarget
 * @property {string} suffix The raw suffix supplied by the user.
 * @property {number} line 1-based line number.
 * @property {number} [column] 1-based column number.
 */

/**
 * Location where the probe was evaluated, serialized into the public report.
 * @typedef {object} Location
 * @property {string} url V8-reported script URL.
 * @property {number} line 1-based line number.
 * @property {number} column 1-based column number.
 */

/**
 * Per-breakpoint state keyed by V8 `breakpointId` from `Debugger.setBreakpointByUrl`.
 * @typedef {object} BreakpointDefinition
 * @property {number[]} probeIndices Indices into probes that bound to this breakpoint.
 */

/**
 * Per-probe state corresponds to each --probe --expr pair.
 * @typedef {object} Probe
 * @property {string} expr Expression to evaluate on hit.
 * @property {ProbeTarget} target User's original --probe request shape.
 * @property {number} hits Count of hits observed.
 */

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

/**
 * @param {string} text Raw `--probe` argument.
 * @returns {ProbeTarget}
 */
function parseProbeTarget(text) {
  // Accepts file:line or file:line:column formats.
  // Non-greedy (.+?) allows Windows drive-letter paths like C:\foo.js:10.
  const match = RegExpPrototypeExec(/^(.+?):(\d+)(?::(\d+))?$/, text);
  if (match === null) {
    throw new ERR_DEBUGGER_STARTUP_ERROR(`Invalid probe location: ${text}`);
  }

  const suffix = match[1];
  const line = parseUnsignedInteger(match[2], 'probe location');
  // Column is left as undefined if the user does not supply one.
  const column = match[3] !== undefined ? parseUnsignedInteger(match[3], 'probe location') : undefined;
  return { suffix, line, column };
}

function formatTargetText(target) {
  const { suffix, line, column } = target;
  return column === undefined ? `${suffix}:${line}` : `${suffix}:${line}:${column}`;
}

function formatPendingProbeLocations(probes, pending) {
  const seen = new SafeSet();
  for (const probeIndex of pending) {
    seen.add(formatTargetText(probes[probeIndex].target));
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

function formatHitLocation(location) {
  return `${location.url}:${location.line}:${location.column}`;
}

// Built human-readable text output for probe reports.
function buildProbeTextReport(report) {
  const lines = [];

  for (const result of report.results) {
    if (result.event === 'hit') {
      const probe = report.probes[result.probe];
      // If Debugger.scriptParsed was missed and the URL is unknown, fall back to the user's
      // probe target text for readability. This is unlikely unless there's a bug in V8.
      const locText = (result.location.url !== undefined) ?
        formatHitLocation(result.location) : formatTargetText(probe.target);
      ArrayPrototypePush(lines, `Hit ${result.hit} at ${locText}`);
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

function parseProbeTokens(tokens, args) {
  let port = 0;
  let preview = false;
  let timeout = kProbeDefaultTimeout;
  let json = false;
  let sawSeparator = false;
  let childStartIndex = args.length;
  let pendingTarget;
  let expectedExprIndex = -1;
  const probes = [];

  for (const token of tokens) {
    if (token.kind === 'option-terminator') {
      sawSeparator = true;
      childStartIndex = token.index + 1;
      break;
    }

    if (pendingTarget !== undefined) {
      if (token.kind === 'option' &&
          token.name === 'expr' &&
          token.index === expectedExprIndex &&
          token.value !== undefined) {
        ArrayPrototypePush(probes, {
          expr: token.value,
          target: pendingTarget,
        });
        pendingTarget = undefined;
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
        pendingTarget = parseProbeTarget(token.value);
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

  if (pendingTarget !== undefined) {
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
    /** @type {Map<string, BreakpointDefinition>} keyed by V8 breakpointId. */
    this.breakpointDefinitions = new SafeMap();
    /** @type {Map<string, string>} scriptId -> URL. */
    this.scriptIdToUrl = new SafeMap();
    this.results = [];
    this.timeout = null;
    this.resolveCompletion = null;
    this.completionPromise = new Promise((resolve) => {
      this.resolveCompletion = resolve;
    });
    /** @type {Probe[]} */
    this.probes = ArrayPrototypeMap(options.probes, ({ expr, target }) => ({ expr, target, hits: 0 }));
    this.onChildOutput = FunctionPrototypeBind(this.onChildOutput, this);
    this.onChildExit = FunctionPrototypeBind(this.onChildExit, this);
    this.onClientClose = FunctionPrototypeBind(this.onClientClose, this);
    this.onPaused = FunctionPrototypeBind(this.onPaused, this);
    this.onScriptParsed = FunctionPrototypeBind(this.onScriptParsed, this);
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

    const topFrame = params.callFrames?.[0];
    const callFrameId = topFrame?.callFrameId;
    if (callFrameId === undefined) {
      await this.resume();
      return;
    }

    const { scriptId, lineNumber, columnNumber } = topFrame.location;
    // `Debugger.scriptParsed` should always precede a pause for the same script.
    // It should only be undefined if there's a bug (even in that case, just omit it).
    const location = {
      url: this.scriptIdToUrl.get(scriptId),
      // CDP locations are 0-based, locations in public report are 1-based.
      line: lineNumber + 1,
      column: columnNumber + 1,
    };
    for (const breakpointId of hitBreakpoints) {
      // The breakpoint ID is stable even for scripts parsed after the initial resolution
      // so we can count on it here.
      const definition = this.breakpointDefinitions.get(breakpointId);
      if (definition === undefined) { continue; }

      // Evaluate the expressions in the order they appear on the command line.
      for (const probeIndex of definition.probeIndices) {
        await this.evaluateProbe(callFrameId, probeIndex, location);
      }
    }

    await this.resume();
  }

  async evaluateProbe(callFrameId, probeIndex, location) {
    const probe = this.probes[probeIndex];
    const evaluation = await this.client.callMethod('Debugger.evaluateOnCallFrame', {
      callFrameId,
      expression: probe.expr,
      generatePreview: true,
    });

    probe.hits++;
    const result = { probe: probeIndex, event: 'hit', hit: probe.hits, location };
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
    this.client.on('Debugger.scriptParsed', this.onScriptParsed);
  }

  onScriptParsed(params) {
    // This map grows by the number of scripts parsed, which is limited, and is just a
    // small string -> string map. The lifetime is bounded by probe timeout etc. so cleanup is overkill.
    this.scriptIdToUrl.set(params.scriptId, params.url);
  }

  async bindBreakpoints() {
    const uniqueTargets = new SafeMap();

    for (let probeIndex = 0; probeIndex < this.probes.length; probeIndex++) {
      const { target } = this.probes[probeIndex];
      const key = `${target.suffix}\n${target.line}\n${target.column ?? ''}`;
      let entry = uniqueTargets.get(key);
      if (entry === undefined) {
        entry = { target, probeIndices: [] };
        uniqueTargets.set(key, entry);
      }
      ArrayPrototypePush(entry.probeIndices, probeIndex);
    }

    for (const { target, probeIndices } of uniqueTargets.values()) {
      // On Windows, normalize backslashes to forward slashes so the regex matches
      // V8 script URLs which always use forward slashes.
      const normalizedFile = process.platform === 'win32' ?
        SideEffectFreeRegExpPrototypeSymbolReplace(/\\/g, target.suffix, '/') :
        target.suffix;
      const escapedPath = SideEffectFreeRegExpPrototypeSymbolReplace(
        /([/\\.?*()^${}|[\]])/g,
        normalizedFile,
        '\\$1',
      );
      const params = {
        urlRegex: `^(.*[\\/\\\\])?${escapedPath}$`,
        // CDP locations are 0-based, the probe target from CLI is 1-based.
        lineNumber: target.line - 1,
      };
      if (target.column !== undefined) {
        // Only pass columnNumber to CDP when the user specifies one, otherwise let
        // the inspector bind to the first executable column.
        params.columnNumber = target.column - 1;
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
      probes: ArrayPrototypeMap(this.probes, ({ expr, target }) => ({ expr, target })),
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
      // On Debugger.enable, V8 emits Debugger.scriptParsed for all existing scripts.
      // Attach the listener early to make sure we don't miss any events.
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

module.exports = {
  parseProbeTokens,
  runProbeMode,
};
