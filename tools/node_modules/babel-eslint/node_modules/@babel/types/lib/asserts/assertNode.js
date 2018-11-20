"use strict";

exports.__esModule = true;
exports.default = assertNode;

var _isNode = _interopRequireDefault(require("../validators/isNode"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function assertNode(node) {
  if (!(0, _isNode.default)(node)) {
    var type = node && node.type || JSON.stringify(node);
    throw new TypeError("Not a valid node of type \"" + type + "\"");
  }
}