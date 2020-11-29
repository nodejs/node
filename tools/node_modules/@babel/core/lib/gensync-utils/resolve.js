"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

function _resolve() {
  const data = _interopRequireDefault(require("resolve"));

  _resolve = function () {
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

var _default = (0, _gensync().default)({
  sync: _resolve().default.sync,
  errback: _resolve().default
});

exports.default = _default;