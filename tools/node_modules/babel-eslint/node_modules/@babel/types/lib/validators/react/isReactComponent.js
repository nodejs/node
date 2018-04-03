"use strict";

exports.__esModule = true;
exports.default = void 0;

var _buildMatchMemberExpression = _interopRequireDefault(require("../buildMatchMemberExpression"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

var isReactComponent = (0, _buildMatchMemberExpression.default)("React.Component");
var _default = isReactComponent;
exports.default = _default;