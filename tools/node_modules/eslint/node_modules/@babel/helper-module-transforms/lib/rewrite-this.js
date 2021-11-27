"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = rewriteThis;

var _helperReplaceSupers = require("@babel/helper-replace-supers");

var _traverse = require("@babel/traverse");

var _t = require("@babel/types");

const {
  numericLiteral,
  unaryExpression
} = _t;

function rewriteThis(programPath) {
  (0, _traverse.default)(programPath.node, Object.assign({}, rewriteThisVisitor, {
    noScope: true
  }));
}

const rewriteThisVisitor = _traverse.default.visitors.merge([_helperReplaceSupers.environmentVisitor, {
  ThisExpression(path) {
    path.replaceWith(unaryExpression("void", numericLiteral(0), true));
  }

}]);