"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.beginHiddenCallStack = beginHiddenCallStack;
exports.endHiddenCallStack = endHiddenCallStack;
exports.expectedError = expectedError;
exports.injcectVirtualStackFrame = injcectVirtualStackFrame;
const ErrorToString = Function.call.bind(Error.prototype.toString);
const SUPPORTED = !!Error.captureStackTrace;
const START_HIDNG = "startHiding - secret - don't use this - v1";
const STOP_HIDNG = "stopHiding - secret - don't use this - v1";
const expectedErrors = new WeakSet();
const virtualFrames = new WeakMap();
function CallSite(filename) {
  return Object.create({
    isNative: () => false,
    isConstructor: () => false,
    isToplevel: () => true,
    getFileName: () => filename,
    getLineNumber: () => undefined,
    getColumnNumber: () => undefined,
    getFunctionName: () => undefined,
    getMethodName: () => undefined,
    getTypeName: () => undefined,
    toString: () => filename
  });
}
function injcectVirtualStackFrame(error, filename) {
  if (!SUPPORTED) return;
  let frames = virtualFrames.get(error);
  if (!frames) virtualFrames.set(error, frames = []);
  frames.push(CallSite(filename));
  return error;
}
function expectedError(error) {
  if (!SUPPORTED) return;
  expectedErrors.add(error);
  return error;
}
function beginHiddenCallStack(fn) {
  if (!SUPPORTED) return fn;
  return Object.defineProperty(function (...args) {
    setupPrepareStackTrace();
    return fn(...args);
  }, "name", {
    value: STOP_HIDNG
  });
}
function endHiddenCallStack(fn) {
  if (!SUPPORTED) return fn;
  return Object.defineProperty(function (...args) {
    return fn(...args);
  }, "name", {
    value: START_HIDNG
  });
}
function setupPrepareStackTrace() {
  setupPrepareStackTrace = () => {};
  const {
    prepareStackTrace = defaultPrepareStackTrace
  } = Error;
  const MIN_STACK_TRACE_LIMIT = 50;
  Error.stackTraceLimit && (Error.stackTraceLimit = Math.max(Error.stackTraceLimit, MIN_STACK_TRACE_LIMIT));
  Error.prepareStackTrace = function stackTraceRewriter(err, trace) {
    let newTrace = [];
    const isExpected = expectedErrors.has(err);
    let status = isExpected ? "hiding" : "unknown";
    for (let i = 0; i < trace.length; i++) {
      const name = trace[i].getFunctionName();
      if (name === START_HIDNG) {
        status = "hiding";
      } else if (name === STOP_HIDNG) {
        if (status === "hiding") {
          status = "showing";
          if (virtualFrames.has(err)) {
            newTrace.unshift(...virtualFrames.get(err));
          }
        } else if (status === "unknown") {
          newTrace = trace;
          break;
        }
      } else if (status !== "hiding") {
        newTrace.push(trace[i]);
      }
    }
    return prepareStackTrace(err, newTrace);
  };
}
function defaultPrepareStackTrace(err, trace) {
  if (trace.length === 0) return ErrorToString(err);
  return `${ErrorToString(err)}\n    at ${trace.join("\n    at ")}`;
}
0 && 0;

//# sourceMappingURL=rewrite-stack-trace.js.map
