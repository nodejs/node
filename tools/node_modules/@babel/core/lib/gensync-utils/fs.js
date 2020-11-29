"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.stat = exports.exists = exports.readFile = void 0;

function _fs() {
  const data = _interopRequireDefault(require("fs"));

  _fs = function () {
    return data;
  };

  return data;
}

function _gensync() {
  const data = _interopRequireDefault(require("gensync"));

  _gensync = function () {
    return data;
  };

  return data;
}

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

const readFile = (0, _gensync().default)({
  sync: _fs().default.readFileSync,
  errback: _fs().default.readFile
});
exports.readFile = readFile;
const exists = (0, _gensync().default)({
  sync(path) {
    try {
      _fs().default.accessSync(path);

      return true;
    } catch (_unused) {
      return false;
    }
  },

  errback: (path, cb) => _fs().default.access(path, undefined, err => cb(null, !err))
});
exports.exists = exists;
const stat = (0, _gensync().default)({
  sync: _fs().default.statSync,
  errback: _fs().default.stat
});
exports.stat = stat;