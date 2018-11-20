"use strict";

exports.__esModule = true;
exports.default = toSequenceExpression;

var _gatherSequenceExpressions = _interopRequireDefault(require("./gatherSequenceExpressions"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function toSequenceExpression(nodes, scope) {
  if (!nodes || !nodes.length) return;
  var declars = [];
  var result = (0, _gatherSequenceExpressions.default)(nodes, scope, declars);
  if (!result) return;

  for (var _i = 0; _i < declars.length; _i++) {
    var declar = declars[_i];
    scope.push(declar);
  }

  return result;
}