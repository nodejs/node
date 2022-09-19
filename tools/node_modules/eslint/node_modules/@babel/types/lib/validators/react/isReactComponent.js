"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

var _buildMatchMemberExpression = require("../buildMatchMemberExpression");

const isReactComponent = (0, _buildMatchMemberExpression.default)("React.Component");
var _default = isReactComponent;
exports.default = _default;

//# sourceMappingURL=isReactComponent.js.map
