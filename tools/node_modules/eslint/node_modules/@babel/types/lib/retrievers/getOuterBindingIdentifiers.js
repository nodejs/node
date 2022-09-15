"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

var _getBindingIdentifiers = require("./getBindingIdentifiers");

var _default = getOuterBindingIdentifiers;
exports.default = _default;

function getOuterBindingIdentifiers(node, duplicates) {
  return (0, _getBindingIdentifiers.default)(node, duplicates, true);
}

//# sourceMappingURL=getOuterBindingIdentifiers.js.map
