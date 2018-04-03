"use strict";

exports.__esModule = true;
exports.default = ensureBlock;

var _toBlock = _interopRequireDefault(require("./toBlock"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function ensureBlock(node, key) {
  if (key === void 0) {
    key = "body";
  }

  return node[key] = (0, _toBlock.default)(node[key], node);
}