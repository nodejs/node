"use strict";

exports.__esModule = true;
exports.default = is;

var _shallowEqual = _interopRequireDefault(require("../utils/shallowEqual"));

var _isType = _interopRequireDefault(require("./isType"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function is(type, node, opts) {
  if (!node) return false;
  var matches = (0, _isType.default)(node.type, type);
  if (!matches) return false;

  if (typeof opts === "undefined") {
    return true;
  } else {
    return (0, _shallowEqual.default)(node, opts);
  }
}