"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = prependToMemberExpression;
var _generated = require("../builders/generated");
var _ = require("..");
function prependToMemberExpression(member, prepend) {
  if ((0, _.isSuper)(member.object)) {
    throw new Error("Cannot prepend node to super property access (`super.foo`).");
  }
  member.object = (0, _generated.memberExpression)(prepend, member.object);
  return member;
}

//# sourceMappingURL=prependToMemberExpression.js.map
