'use strict';

const {
  ArrayFrom,
  ArrayIsArray,
  ArrayPrototypeJoin,
  ArrayPrototypeMap,
  ArrayPrototypePush,
  ArrayPrototypeSlice,
  ArrayPrototypeSome,
  FunctionPrototypeBind,
  JSONStringify,
  NumberIsNaN,
  NumberParseInt,
  ObjectEntries,
  PromiseWithResolvers,
  RegExpPrototypeExec,
  RegExpPrototypeSymbolSplit,
  SafeMap,
  SafeSet,
  StringPrototypeIncludes,
  StringPrototypeSlice,
  StringPrototypeStartsWith,
  Symbol,
} = primordials;

const { clearTimeout, setTimeout } = require('timers');
const { SideEffectFreeRegExpPrototypeSymbolReplace } = require('internal/util');
const debug = require('internal/util/debuglog').debuglog('inspect_probe');

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
const kProbeAttachedSentinel = 'Debugger attached.';
const kProbeListeningPrefix = 'Debugger listening on ws://';
const kProbeEndingPrefix = 'Debugger ending on ws://';
const kProbeHelpLine =
  'For help, see: https://nodejs.org/learn/getting-started/debugging';
// Thrown by `callCdp` after `recordInspectorFailure` has handled the failure,
// so callers can short-circuit without recording duplicate events.
const kInspectorFailedSentinel = Symbol('probe.inspectorFailed');
const kStartupTeardownAdvice =
  'The target startup may have torn down the inspector. If startup does ' +
  'not touch the inspector, this is likely a Node.js bug. Please file an issue.';
const kReviewProbeExprAdvice = 'If the failure repeats, review the probe expression.';
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
 * @property {string} [url] V8-reported script URL, if known.
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
 * @property {number} maxHit Per-probe hit limit from --max-hit. Infinity when unlimited.
 * @property {number} hits Count of hits observed.
 */

function probeReachedLimit(probe) {
  return probe.hits >= probe.maxHit;
}

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

// Trim inspector-side noise lines from stderr for reporting child errors.
function trimProbeChildStderr(stderr) {
  const lines = RegExpPrototypeSymbolSplit(/\r\n|\r|\n/g, stderr);
  const kept = [];
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];
    if (line === '' && i === lines.length - 1) { continue; }
    if (line === kProbeDisconnectSentinel) { continue; }
    if (line === kProbeAttachedSentinel) { continue; }
    if (line === kProbeHelpLine) { continue; }
    if (StringPrototypeStartsWith(line, kProbeListeningPrefix)) { continue; }
    if (StringPrototypeStartsWith(line, kProbeEndingPrefix)) { continue; }
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
        ArrayPrototypePush(lines, `  [error] ${probe.expr} = ${result.error.message}`);
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
      case 'max-hit': {
        if (probes.length === 0) {
          throw new ERR_DEBUGGER_STARTUP_ERROR('Unexpected --max-hit before --probe');
        }
        if (token.value === undefined) {
          throw new ERR_DEBUGGER_STARTUP_ERROR(`Missing value for ${token.rawName}`);
        }
        const probe = probes[probes.length - 1];
        if (probe.maxHit !== undefined) {
          throw new ERR_DEBUGGER_STARTUP_ERROR('Duplicate --max-hit for a single --probe');
        }
        probe.maxHit = parseUnsignedInteger(token.value, 'max-hit');
        break;
      }
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
    // True once the inspector WebSocket connects. Event handlers ignore
    // pre-connect exits/closes so launch/connect failures report through run().
    this.connected = false;
    // True once breakpoints are bound and the target is released from --inspect-brk
    // via `Runtime.runIfWaitingForDebugger`.
    // Distinguishes "exited before user code ran" from "exited during the live session".
    this.started = false;
    // A sliding buffer of at most kProbeDisconnectSentinel.length to detect disconnect.
    this.disconnectSentinelBuffer = '';
    /** @type {Map<string, BreakpointDefinition>} keyed by V8 breakpointId. */
    this.breakpointDefinitions = new SafeMap();
    /** @type {Map<string, string>} scriptId -> URL. */
    this.scriptIdToUrl = new SafeMap();
    this.results = [];
    this.timeout = null;
    // The currently-awaited CDP request, or null when no request is in flight.
    /** @type {{ method: string, probe: { index: number, location: Location } | null } | null} */
    this.inFlight = null;
    // Most recently completed probe, used for `error.probe` when a non-evaluate CDP call rejects.
    this.lastProbeIndex = null;
    this.deferredStartupPause = null;
    this.handledPauseAfterStart = false;
    const { promise, resolve } = PromiseWithResolvers();
    this.completionPromise = promise;
    this.resolveCompletion = resolve;
    /** @type {Probe[]} */
    this.probes = ArrayPrototypeMap(options.probes,
                                    ({ expr, target, maxHit }) =>
                                      ({ expr, target, maxHit: maxHit ?? Infinity, hits: 0 }));
    this.onChildOutput = FunctionPrototypeBind(this.onChildOutput, this);
    this.onChildExit = FunctionPrototypeBind(this.onChildExit, this);
    this.onClientClose = FunctionPrototypeBind(this.onClientClose, this);
    this.onPaused = FunctionPrototypeBind(this.onPaused, this);
    this.onScriptParsed = FunctionPrototypeBind(this.onScriptParsed, this);
  }

  // Marking the probing process to exit with 0, recorded hits are trustworthy.
  finishWithTrustedResult(terminal) {
    this.finish(kNoFailure, terminal);
  }

  // Marking the probing process to exit with 1, recorded hits are best-effort.
  finishWithUnreliableResult(terminal) {
    this.finish(kGenericUserError, terminal);
  }

  finish(exitCode, terminal) {
    if (this.finished) { return; }
    debug('finish: exitCode=%d, terminal=%s', exitCode, terminal?.event);
    this.finished = true;
    if (this.timeout !== null) {
      clearTimeout(this.timeout);
      this.timeout = null;
    }
    this.resolveCompletion({ exitCode, terminal });
  }

  getProbeTargetExitEvent(exitCode, signal) {
    const pending = this.getPendingProbeIndices();
    const how = signal !== null ? `with signal ${signal}` : `with code ${exitCode}`;
    const status = pending.length === 0 ?
      'target completion' : `probes: ${formatPendingProbeLocations(this.probes, pending)}`;
    const error = {
      __proto__: null,
      code: 'probe_target_exit',
      message: `Target exited ${how} before ${status}`,
    };
    if (exitCode !== null) { error.exitCode = exitCode; }
    if (signal !== null) { error.signal = signal; }
    error.stderr = trimProbeChildStderr(this.childStderr);
    return { event: 'error', pending, error };
  }

  onChildOutput(text, which) {
    if (which !== 'stderr') { return; }
    debug('child stderr: %j', text);

    this.childStderr += text;

    const combined = this.disconnectSentinelBuffer + text;
    // Detect the disconnect sentinel.
    if (this.connected &&
        StringPrototypeIncludes(combined, kProbeDisconnectSentinel)) {
      debug('disconnect sentinel detected, resetting client');
      this.disconnectRequested = true;
      this.client.reset();
    }

    if (combined.length > kProbeDisconnectSentinel.length) {  // Slide the buffer.
      this.disconnectSentinelBuffer = StringPrototypeSlice(combined,
                                                           combined.length - kProbeDisconnectSentinel.length);
    } else {
      this.disconnectSentinelBuffer = combined;
    }
  }

  onChildExit(code, signal) {
    debug('child exit: code=%s signal=%s connected=%s started=%s finished=%s inFlight=%j',
          code, signal, this.connected, this.started, this.finished, this.inFlight);
    // Pre-connect exits are deliberately silent: the target never reached
    // a state where probes could be set, so any report would be empty.
    if (!this.connected) { return; }
    if (this.finished) { return; }
    if (this.inFlight !== null && this.inFlight.probe !== null) {
      this.recordInspectorFailure({
        reason: 'Target process exited during probe evaluation',
        advice: kReviewProbeExprAdvice,
      });
      return;
    }
    if (this.started && code === 0 && signal === null) {
      const pending = this.getPendingProbeIndices();
      this.finishWithTrustedResult(pending.length === 0 ? { event: 'completed' } : { event: 'miss', pending });
      return;
    }
    this.finishWithTrustedResult(this.getProbeTargetExitEvent(code, signal));
  }

  onClientClose() {
    debug('client close: disconnectRequested=%s finished=%s inFlight=%j',
          this.disconnectRequested, this.finished, this.inFlight);
    if (!this.connected) { return; }
    if (this.disconnectRequested) { return; }
    if (this.finished) { return; }

    if (this.child.exitCode !== null || this.child.signalCode !== null) {
      this.onChildExit(this.child.exitCode, this.child.signalCode);
      return;
    }
    if (!this.started) {
      this.recordInspectorFailure({
        reason: 'Inspector connection lost before probes started',
        advice: kStartupTeardownAdvice,
      });
      return;
    }
    if (this.inFlight !== null) {
      if (this.inFlight.probe !== null || this.lastProbeIndex !== null) {
        this.recordInspectorFailure({
          reason: 'Inspector connection lost during probe activity',
          advice: 'A probe expression may have caused the disconnection. ' +
            'If the failure repeats, review the probe expressions.',
        });
      } else {
        this.recordInspectorFailure({
          reason: 'Inspector connection lost during inspector activity',
          advice: 'This is likely a Node.js bug. Please file an issue.',
        });
      }
      return;
    }
    this.recordInspectorFailure({
      reason: 'Inspector connection lost',
      advice: 'The target was likely terminated externally. If the failure ' +
        'persists, check the target\'s process environment.',
    });
  }

  onPaused(params) {
    debug('paused: finished=%d, reason=%s hitBreakpoints=%j', this.finished, params.reason, params.hitBreakpoints);
    this.handlePaused(params).catch((error) => {
      if (error === kInspectorFailedSentinel) { return; }
      this.recordInspectorFailure({
        reason: 'Probe mode encountered an unexpected internal failure',
        advice: 'This is likely a Node.js bug. Please file an issue.',
        internalError: error,
      });
    });
  }

  async handlePaused(params) {
    if (this.finished) { return; }
    // Defer pauses that arrive before breakpoint setup is complete. Once
    // startup is marked complete, `Runtime.runIfWaitingForDebugger` may surface
    // the initial --inspect-brk pause, which the normal resume path handles. If
    // it was already surfaced while binding breakpoints, replay it after startup
    // so the target is not left paused forever.
    if (!this.started) {
      this.deferredStartupPause = params;
      return;
    }

    this.handledPauseAfterStart = true;

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
        if (this.finished) { break; }
      }
    }

    // Finish proactively as soon as any probe reaches its hit limit. All probes
    // hit in this pause are recorded first, then the session ends.
    // TODO(joyeecheung): When we implement attach mode, this teardown must
    // resume-and-detach rather than kill, since the target is not ours.
    if (!this.finished && ArrayPrototypeSome(this.probes, probeReachedLimit)) {
      this.finishWithTrustedResult({ event: 'completed' });
      return;
    }

    await this.resume();
  }

  async handleDeferredStartupPause() {
    if (this.deferredStartupPause === null || this.handledPauseAfterStart) {
      return;
    }

    const params = this.deferredStartupPause;
    this.deferredStartupPause = null;
    await this.handlePaused(params);
  }

  async evaluateProbe(callFrameId, probeIndex, location) {
    if (this.finished) { return; }
    const probe = this.probes[probeIndex];
    const evaluation = await this.callCdp(
      'Debugger.evaluateOnCallFrame',
      { callFrameId, expression: probe.expr, generatePreview: true },
      { __proto__: null, index: probeIndex, location },
    );
    this.lastProbeIndex = probeIndex;

    probe.hits++;
    const result = { probe: probeIndex, event: 'hit', hit: probe.hits, location };
    if (evaluation.exceptionDetails !== undefined) {
      result.error = {
        __proto__: null,
        message: evaluation.result?.description ?? 'Probe expression failed',
        details: { __proto__: null, exception: trimRemoteObject(evaluation.exceptionDetails) },
      };
    } else {
      result.result = trimRemoteObject(evaluation.result);
    }
    ArrayPrototypePush(this.results, result);
  }

  async resume() {
    if (this.finished) { return; }
    await this.callCdp('Debugger.resume');
  }

  async callCdp(method, params, probe = null) {
    if (this.finished) { throw kInspectorFailedSentinel; }
    this.inFlight = { __proto__: null, method, probe };
    debug('CDP -> %s%s', method, probe !== null ? `, probe=${probe.index}` : '');
    try {
      const result = await this.client.callMethod(method, params);
      // A timeout or process exit can finish the report while the CDP request
      // is still outstanding. Ignore the late reply in that case.
      if (this.finished) {
        debug('CDP <- %s discarded (already finished)', method);
        throw kInspectorFailedSentinel;
      }
      debug('CDP <- %s (success)', method);
      return result;
    } catch (err) {
      if (err !== kInspectorFailedSentinel) {  // Already handled.
        debug('CDP <- %s error: %s', method, err?.code);
      }
      if (this.disconnectRequested) {
        // Only the in-flight evaluation gets attribution. Other rejections
        // under disconnect are downstream noise.
        if (probe !== null) {
          this.recordInspectorFailure({
            reason: 'Target process exited during probe evaluation',
            advice: kReviewProbeExprAdvice,
          });
        }
        throw kInspectorFailedSentinel;
      }
      // Another event handler already recorded the terminal event.
      if (this.finished) { throw kInspectorFailedSentinel; }
      if (!this.started) {
        this.recordInspectorFailure({
          reason: 'Probe mode failed before user code ran',
          advice: kStartupTeardownAdvice,
          cdpError: err,
        });
      } else if (method === 'Debugger.evaluateOnCallFrame') {
        this.recordInspectorFailure({
          reason: 'The inspector could not evaluate a probe expression',
          advice: `The rejection details are recorded on the probe hit. ${kReviewProbeExprAdvice}`,
          cdpError: err,
        });
      } else if (this.lastProbeIndex !== null) {
        this.recordInspectorFailure({
          reason: 'Probe session failed after a probe evaluation',
          advice: 'If the failure repeats, review the most-recently-evaluated probe expression.',
          cdpError: err,
        });
      } else {
        this.recordInspectorFailure({
          reason: 'Probe session failed during inspector activity',
          advice: 'This is likely a Node.js bug. Please file an issue.',
          cdpError: err,
        });
      }
      throw kInspectorFailedSentinel;
    } finally {
      this.inFlight = null;
    }
  }

  // Records the first inspector-side terminal for the session, later callers are ignored.
  recordInspectorFailure({ reason, advice, cdpError, internalError }) {
    if (this.finished) { return; }
    debug('recordInspectorFailure "%s": inFlight=%j, lastProbeIndex=%s, cdpError=%j',
          reason, this.inFlight, this.lastProbeIndex, cdpError);
    const child = this.child;
    const exitedAbnormally = child !== null &&
      (child.signalCode !== null || (child.exitCode !== null && child.exitCode !== 0));
    const inFlightProbe = this.inFlight === null ? null : this.inFlight.probe;
    // This normally emits `probe_failure`, but yields to `probe_target_exit` when the child
    // has already exited abnormally and there is no in-flight probe to attribute to.
    if (exitedAbnormally && inFlightProbe === null) {
      this.finishWithTrustedResult(this.getProbeTargetExitEvent(child.exitCode, child.signalCode));
      return;
    }

    const failedCdpMethod = this.inFlight === null ? null : this.inFlight.method;
    let protocolError = null;
    // // `ERR_DEBUGGER_ERROR` is a Node-internal code, not a CDP-level protocol code
    if (cdpError !== undefined && cdpError.code !== 'ERR_DEBUGGER_ERROR') {
      protocolError = { __proto__: null, message: cdpError.message, code: cdpError.code };
    }
    const protocolErrorGoesOnHit = (protocolError !== null) && (failedCdpMethod === 'Debugger.evaluateOnCallFrame');

    let attribution = null;
    if (inFlightProbe !== null) {
      const { index, location } = inFlightProbe;
      const error = { __proto__: null };
      if (protocolErrorGoesOnHit) {
        error.message = 'Probe evaluation failed at the protocol layer';
        error.details = { __proto__: null, protocolError };
      } else {
        error.message = 'Probe evaluation did not complete';
      }
      this.probes[index].hits++;
      ArrayPrototypePush(this.results, {
        probe: index, event: 'hit', hit: this.probes[index].hits, location, error,
      });
      attribution = index;
    } else if (failedCdpMethod !== null && this.lastProbeIndex !== null) {
      attribution = this.lastProbeIndex;
    }
    // When there is no in-flight CDP call (e.g. `onClientClose` after all probes hit), ignore
    // `lastProbeIndex` since it can't be attributed to a specific probe.

    const pending = this.getPendingProbeIndices();
    const suffix = pending.length === 0 ?
      '' : ` before probes: ${formatPendingProbeLocations(this.probes, pending)}`;
    const error = {
      __proto__: null,
      code: 'probe_failure',
      message: `${reason}${suffix}. ${advice}`,
    };
    if (attribution !== null) { error.probe = attribution; }
    error.stderr = trimProbeChildStderr(this.childStderr);

    let details;
    if (failedCdpMethod !== null) {
      details = { __proto__: null, lastCdpMethod: failedCdpMethod };
      if (protocolError !== null && !protocolErrorGoesOnHit) { details.protocolError = protocolError; }
    }
    if (internalError !== undefined) {
      details ??= { __proto__: null };
      details.internalError = { __proto__: null, message: internalError?.message, stack: internalError?.stack };
    }
    if (details !== undefined) { error.details = details; }

    this.finishWithUnreliableResult({ event: 'error', pending, error });
  }

  startTimeout() {
    this.timeout = setTimeout(() => {
      debug('timeout fired: finished=%s, inFlight=%j, lastProbeIndex=%s',
            this.finished, this.inFlight, this.lastProbeIndex);
      if (this.finished) { return; }
      if (this.inFlight !== null) {
        const hasProbeAttribution =
          this.inFlight.probe !== null || this.lastProbeIndex !== null;
        this.recordInspectorFailure({
          reason: 'Probe session timed out',
          advice: hasProbeAttribution ?
            ('The probe expression may be slow, hanging, or interfering with the inspector connection. ' +
            'Try increasing `--timeout`; if the failure persists, review the probe expressions.') :
            'Try increasing `--timeout`; if the failure persists, please file an issue.',
        });
        return;
      }
      const pending = this.getPendingProbeIndices();
      const message = `Timed out after ${this.options.timeout}ms waiting for ` +
        (pending.length === 0 ? 'target completion' :
          `probes: ${formatPendingProbeLocations(this.probes, pending)}`);
      this.finishWithUnreliableResult({
        event: 'timeout',
        pending,
        error: { code: 'probe_timeout', message },
      });
    }, this.options.timeout);
    this.timeout.unref();
  }

  attachListeners() {
    this.child.on('exit', this.onChildExit);
    this.client.on('close', this.onClientClose);
    this.client.on('Debugger.paused', this.onPaused);
    this.client.on('Debugger.scriptParsed', this.onScriptParsed);
  }

  onScriptParsed(params) {
    if (params.url && !StringPrototypeStartsWith(params.url, 'node:')) {
      debug('scriptParsed: scriptId=%s url=%s, length=%d', params.scriptId, params.url, params.length);
    }
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

      const result = await this.callCdp('Debugger.setBreakpointByUrl', params);
      debug('breakpoint set: id=%s urlRegex=%s locations=%j',
            result.breakpointId, params.urlRegex, result.locations);
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

  buildReport({ exitCode, terminal }) {
    const results = ArrayPrototypeSlice(this.results);
    ArrayPrototypePush(results, terminal);
    return {
      code: exitCode,
      report: {
        v: kProbeVersion,
        probes: ArrayPrototypeMap(this.probes, ({ expr, target, maxHit }) => {
          // Omit an unlimited maxHit, as Infinity would serialize to null in JSON.
          const probe = { expr, target };
          if (maxHit !== Infinity) { probe.maxHit = maxHit; }
          return probe;
        }),
        results,
      },
    };
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

    // TODO(joyeecheung): When we implement attach mode, this teardown must
    // resume-and-detach rather than kill, since the target is not ours.
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
      this.connected = true;

      try {
        await this.callCdp('Runtime.enable');
        await this.callCdp('Debugger.enable');
        await this.bindBreakpoints();
        this.started = true;
        this.startTimeout();
        await this.callCdp('Runtime.runIfWaitingForDebugger');
        await this.handleDeferredStartupPause();
      } catch (err) {
        if (err !== kInspectorFailedSentinel) { throw err; }
      }

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
  ProbeInspectorSession,
  runProbeMode,
};
