"use strict";

exports.__esModule = true;
exports.default = cloneWithoutLoc;

var _clone = _interopRequireDefault(require("./clone"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function cloneWithoutLoc(node) {
  var newNode = (0, _clone.default)(node);
  newNode.loc = null;
  return newNode;
}