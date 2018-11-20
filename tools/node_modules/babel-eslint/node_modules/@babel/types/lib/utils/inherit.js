"use strict";

exports.__esModule = true;
exports.default = inherit;

var _uniq = _interopRequireDefault(require("lodash/uniq"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function inherit(key, child, parent) {
  if (child && parent) {
    child[key] = (0, _uniq.default)([].concat(child[key], parent[key]).filter(Boolean));
  }
}