"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = _default;

function _v() {
  const data = require("v8");

  _v = function () {
    return data;
  };

  return data;
}

var _cloneDeepBrowser = require("./clone-deep-browser");

function _default(value) {
  if (_v().deserialize && _v().serialize) {
    return _v().deserialize(_v().serialize(value));
  }

  return (0, _cloneDeepBrowser.default)(value);
}