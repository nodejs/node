"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = deprecationWarning;
const warnings = new Set();
function deprecationWarning(oldName, newName, prefix = "") {
  if (warnings.has(oldName)) return;
  warnings.add(oldName);
  const stack = captureShortStackTrace(1, 2);
  console.warn(`${prefix}\`${oldName}\` has been deprecated, please migrate to \`${newName}\`\n${stack}`);
}
function captureShortStackTrace(skip, length) {
  const {
    stackTraceLimit,
    prepareStackTrace
  } = Error;
  let stackTrace;
  Error.stackTraceLimit = 1 + skip + length;
  Error.prepareStackTrace = function (err, stack) {
    stackTrace = stack;
  };
  new Error().stack;
  Error.stackTraceLimit = stackTraceLimit;
  Error.prepareStackTrace = prepareStackTrace;
  return stackTrace.slice(1 + skip, 1 + skip + length).map(frame => `    at ${frame}`).join("\n");
}

//# sourceMappingURL=deprecationWarning.js.map
