"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.transformFromAst = void 0;
exports.transformFromAstAsync = transformFromAstAsync;
exports.transformFromAstSync = transformFromAstSync;
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
const transformFromAstRunner = _gensync()(function* (ast, code, opts) {
  const config = yield* (0, _config.default)(opts);
  if (config === null) return null;
  if (!ast) throw new Error("No AST given");
  return yield* (0, _transformation.run)(config, code, ast);
});
const transformFromAst = function transformFromAst(ast, code, optsOrCallback, maybeCallback) {
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
      return (0, _rewriteStackTrace.beginHiddenCallStack)(transformFromAstRunner.sync)(ast, code, opts);
    }
  }
  (0, _rewriteStackTrace.beginHiddenCallStack)(transformFromAstRunner.errback)(ast, code, opts, callback);
};
exports.transformFromAst = transformFromAst;
function transformFromAstSync(...args) {
  return (0, _rewriteStackTrace.beginHiddenCallStack)(transformFromAstRunner.sync)(...args);
}
function transformFromAstAsync(...args) {
  return (0, _rewriteStackTrace.beginHiddenCallStack)(transformFromAstRunner.async)(...args);
}
0 && 0;

//# sourceMappingURL=transform-ast.js.map
