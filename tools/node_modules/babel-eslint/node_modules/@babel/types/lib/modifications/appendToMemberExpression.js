"use strict";

exports.__esModule = true;
exports.default = appendToMemberExpression;

var _generated = require("../builders/generated");

function appendToMemberExpression(member, append, computed) {
  if (computed === void 0) {
    computed = false;
  }

  member.object = (0, _generated.memberExpression)(member.object, member.property, member.computed);
  member.property = append;
  member.computed = !!computed;
  return member;
}