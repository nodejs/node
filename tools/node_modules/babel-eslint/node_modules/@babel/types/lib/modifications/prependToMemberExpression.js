"use strict";

exports.__esModule = true;
exports.default = prependToMemberExpression;

var _generated = require("../builders/generated");

function prependToMemberExpression(member, prepend) {
  member.object = (0, _generated.memberExpression)(prepend, member.object);
  return member;
}