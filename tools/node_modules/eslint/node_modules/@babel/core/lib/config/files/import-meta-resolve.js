"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = resolve;
var _importMetaResolve = require("../../vendor/import-meta-resolve");
function asyncGeneratorStep(gen, resolve, reject, _next, _throw, key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { Promise.resolve(value).then(_next, _throw); } }
function _asyncToGenerator(fn) { return function () { var self = this, args = arguments; return new Promise(function (resolve, reject) { var gen = fn.apply(self, args); function _next(value) { asyncGeneratorStep(gen, resolve, reject, _next, _throw, "next", value); } function _throw(err) { asyncGeneratorStep(gen, resolve, reject, _next, _throw, "throw", err); } _next(undefined); }); }; }
let import_;
try {
  import_ = require("./import.cjs");
} catch (_unused) {}
const importMetaResolveP = import_ && process.execArgv.includes("--experimental-import-meta-resolve") ? import_("data:text/javascript,export default import.meta.resolve").then(m => m.default || _importMetaResolve.resolve, () => _importMetaResolve.resolve) : Promise.resolve(_importMetaResolve.resolve);
function resolve(_x, _x2) {
  return _resolve.apply(this, arguments);
}
function _resolve() {
  _resolve = _asyncToGenerator(function* (specifier, parent) {
    return (yield importMetaResolveP)(specifier, parent);
  });
  return _resolve.apply(this, arguments);
}
0 && 0;

//# sourceMappingURL=import-meta-resolve.js.map
