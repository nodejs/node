"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = rewriteThis;

var _helperReplaceSupers = require("@babel/helper-replace-supers");

var _traverse = _interopRequireDefault(require("@babel/traverse"));

var t = _interopRequireWildcard(require("@babel/types"));

function _getRequireWildcardCache() { if (typeof WeakMap !== "function") return null; var cache = new WeakMap(); _getRequireWildcardCache = function () { return cache; }; return cache; }

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } if (obj === null || typeof obj !== "object" && typeof obj !== "function") { return { default: obj }; } var cache = _getRequireWildcardCache(); if (cache && cache.has(obj)) { return cache.get(obj); } var newObj = {}; var hasPropertyDescriptor = Object.defineProperty && Object.getOwnPropertyDescriptor; for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = hasPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : null; if (desc && (desc.get || desc.set)) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } newObj.default = obj; if (cache) { cache.set(obj, newObj); } return newObj; }

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function rewriteThis(programPath) {
  (0, _traverse.default)(programPath.node, Object.assign({}, rewriteThisVisitor, {
    noScope: true
  }));
}

const rewriteThisVisitor = _traverse.default.visitors.merge([_helperReplaceSupers.environmentVisitor, {
  ThisExpression(path) {
    path.replaceWith(t.unaryExpression("void", t.numericLiteral(0), true));
  }

}]);