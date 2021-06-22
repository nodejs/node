"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = optimiseCallExpression;

var t = require("@babel/types");

function optimiseCallExpression(callee, thisNode, args, optional) {
  if (args.length === 1 && t.isSpreadElement(args[0]) && t.isIdentifier(args[0].argument, {
    name: "arguments"
  })) {
    if (optional) {
      return t.optionalCallExpression(t.optionalMemberExpression(callee, t.identifier("apply"), false, true), [thisNode, args[0].argument], false);
    }

    return t.callExpression(t.memberExpression(callee, t.identifier("apply")), [thisNode, args[0].argument]);
  } else {
    if (optional) {
      return t.optionalCallExpression(t.optionalMemberExpression(callee, t.identifier("call"), false, true), [thisNode, ...args], false);
    }

    return t.callExpression(t.memberExpression(callee, t.identifier("call")), [thisNode, ...args]);
  }
}