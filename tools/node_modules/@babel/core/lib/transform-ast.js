"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.transformFromAstAsync = exports.transformFromAstSync = exports.transformFromAst = void 0;

function _gensync() {
  const data = _interopRequireDefault(require("gensync"));

  _gensync = function () {
    return data;
  };

  return data;
}

var _config = _interopRequireDefault(require("./config"));

var _transformation = require("./transformation");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

const transformFromAstRunner = (0, _gensync().default)(function* (ast, code, opts) {
  const config = yield* (0, _config.default)(opts);
  if (config === null) return null;
  if (!ast) throw new Error("No AST given");
  return yield* (0, _transformation.run)(config, code, ast);
});

const transformFromAst = function transformFromAst(ast, code, opts, callback) {
  if (typeof opts === "function") {
    callback = opts;
    opts = undefined;
  }

  if (callback === undefined) {
    return transformFromAstRunner.sync(ast, code, opts);
  }

  transformFromAstRunner.errback(ast, code, opts, callback);
};

exports.transformFromAst = transformFromAst;
const transformFromAstSync = transformFromAstRunner.sync;
exports.transformFromAstSync = transformFromAstSync;
const transformFromAstAsync = transformFromAstRunner.async;
exports.transformFromAstAsync = transformFromAstAsync;