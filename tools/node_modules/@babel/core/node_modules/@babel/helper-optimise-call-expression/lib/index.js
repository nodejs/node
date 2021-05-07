"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = optimiseCallExpression;

var t = _interopRequireWildcard(require("@babel/types"));

function _getRequireWildcardCache() { if (typeof WeakMap !== "function") return null; var cache = new WeakMap(); _getRequireWildcardCache = function () { return cache; }; return cache; }

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } if (obj === null || typeof obj !== "object" && typeof obj !== "function") { return { default: obj }; } var cache = _getRequireWildcardCache(); if (cache && cache.has(obj)) { return cache.get(obj); } var newObj = {}; var hasPropertyDescriptor = Object.defineProperty && Object.getOwnPropertyDescriptor; for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = hasPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : null; if (desc && (desc.get || desc.set)) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } newObj.default = obj; if (cache) { cache.set(obj, newObj); } return newObj; }

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