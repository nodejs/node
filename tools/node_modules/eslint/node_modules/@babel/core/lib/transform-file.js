"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.transformFileSync = exports.transformFileAsync = exports.transformFile = void 0;

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

const transformFile = transformFileRunner.errback;
exports.transformFile = transformFile;
const transformFileSync = transformFileRunner.sync;
exports.transformFileSync = transformFileSync;
const transformFileAsync = transformFileRunner.async;
exports.transformFileAsync = transformFileAsync;
0 && 0;