"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.transform = void 0;
exports.transformAsync = transformAsync;
exports.transformSync = transformSync;
function _gensync() {
  const data = require("gensync");
  _gensync = function () {
    return data;
  };
  return data;
}
var _config = require("./config");
var _transformation = require("./transformation");
var _rewriteStackTrace = require("./errors/rewrite-stack-trace");
const transformRunner = _gensync()(function* transform(code, opts) {
  const config = yield* (0, _config.default)(opts);
  if (config === null) return null;
  return yield* (0, _transformation.run)(config, code);
});
const transform = function transform(code, optsOrCallback, maybeCallback) {
  let opts;
  let callback;
  if (typeof optsOrCallback === "function") {
    callback = optsOrCallback;
    opts = undefined;
  } else {
    opts = optsOrCallback;
    callback = maybeCallback;
  }
  if (callback === undefined) {
    {
      return (0, _rewriteStackTrace.beginHiddenCallStack)(transformRunner.sync)(code, opts);
    }
  }
  (0, _rewriteStackTrace.beginHiddenCallStack)(transformRunner.errback)(code, opts, callback);
};
exports.transform = transform;
function transformSync(...args) {
  return (0, _rewriteStackTrace.beginHiddenCallStack)(transformRunner.sync)(...args);
}
function transformAsync(...args) {
  return (0, _rewriteStackTrace.beginHiddenCallStack)(transformRunner.async)(...args);
}
0 && 0;

//# sourceMappingURL=transform.js.map
