"use strict";

exports.__esModule = true;
exports.default = inherits;

var _constants = require("../constants");

var _inheritsComments = _interopRequireDefault(require("../comments/inheritsComments"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function inherits(child, parent) {
  if (!child || !parent) return child;
  var _arr = _constants.INHERIT_KEYS.optional;

  for (var _i = 0; _i < _arr.length; _i++) {
    var key = _arr[_i];

    if (child[key] == null) {
      child[key] = parent[key];
    }
  }

  for (var _key in parent) {
    if (_key[0] === "_" && _key !== "__clone") child[_key] = parent[_key];
  }

  var _arr2 = _constants.INHERIT_KEYS.force;

  for (var _i2 = 0; _i2 < _arr2.length; _i2++) {
    var _key2 = _arr2[_i2];
    child[_key2] = parent[_key2];
  }

  (0, _inheritsComments.default)(child, parent);
  return child;
}