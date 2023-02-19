"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.transformFile = transformFile;
exports.transformFileAsync = transformFileAsync;
exports.transformFileSync = transformFileSync;
function _gensync() {
  const data = require("gensync");
  _gensync = function () {
    return data;
  };
  return data;
}
var _config = require("./config");
var _transformation = require("./transformation");
var fs = require("./gensync-utils/fs");
({});
const transformFileRunner = _gensync()(function* (filename, opts) {
  const options = Object.assign({}, opts, {
    filename
  });
  const config = yield* (0, _config.default)(options);
  if (config === null) return null;
  const code = yield* fs.readFile(filename, "utf8");
  return yield* (0, _transformation.run)(config, code);
});
function transformFile(...args) {
  return transformFileRunner.errback(...args);
}
function transformFileSync(...args) {
  return transformFileRunner.sync(...args);
}
function transformFileAsync(...args) {
  return transformFileRunner.async(...args);
}
0 && 0;

//# sourceMappingURL=transform-file.js.map
