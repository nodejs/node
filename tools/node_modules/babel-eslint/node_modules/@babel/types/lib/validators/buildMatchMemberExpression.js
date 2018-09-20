"use strict";

exports.__esModule = true;
exports.default = buildMatchMemberExpression;

var _matchesPattern = _interopRequireDefault(require("./matchesPattern"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function buildMatchMemberExpression(match, allowPartial) {
  var parts = match.split(".");
  return function (member) {
    return (0, _matchesPattern.default)(member, parts, allowPartial);
  };
}