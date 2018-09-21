"use strict";

exports.__esModule = true;
exports.clear = clear;
exports.clearPath = clearPath;
exports.clearScope = clearScope;
exports.scope = exports.path = void 0;
var path = new WeakMap();
exports.path = path;
var scope = new WeakMap();
exports.scope = scope;

function clear() {
  clearPath();
  clearScope();
}

function clearPath() {
  exports.path = path = new WeakMap();
}

function clearScope() {
  exports.scope = scope = new WeakMap();
}