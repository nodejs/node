/*
 * Transpiled with Babel from:
 *
 * export { π as pi } from './exports-cases.js';
 * export default 'the default';
 * export const name = 'name';
 */

"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
Object.defineProperty(exports, "pi", {
  enumerable: true,
  get: function () {
    return _exportsCases.π;
  }
});
exports.name = exports.default = void 0;

var _exportsCases = require("./exports-cases.js");

var _default = 'the default';
exports.default = _default;
const name = 'name';
exports.name = name;

exports.case2 = 'case2';
