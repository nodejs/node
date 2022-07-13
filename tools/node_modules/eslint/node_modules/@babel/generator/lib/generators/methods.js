"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.ArrowFunctionExpression = ArrowFunctionExpression;
exports.FunctionDeclaration = exports.FunctionExpression = FunctionExpression;
exports._functionHead = _functionHead;
exports._methodHead = _methodHead;
exports._param = _param;
exports._parameters = _parameters;
exports._params = _params;
exports._predicate = _predicate;

var _t = require("@babel/types");

const {
  isIdentifier
} = _t;

function _params(node) {
  this.print(node.typeParameters, node);
  this.token("(");

  this._parameters(node.params, node);

  this.token(")");
  this.print(node.returnType, node);
}

function _parameters(parameters, parent) {
  for (let i = 0; i < parameters.length; i++) {
    this._param(parameters[i], parent);

    if (i < parameters.length - 1) {
      this.token(",");
      this.space();
    }
  }
}

function _param(parameter, parent) {
  this.printJoin(parameter.decorators, parameter);
  this.print(parameter, parent);

  if (parameter.optional) {
    this.token("?");
  }

  this.print(parameter.typeAnnotation, parameter);
}

function _methodHead(node) {
  const kind = node.kind;
  const key = node.key;

  if (kind === "get" || kind === "set") {
    this.word(kind);
    this.space();
  }

  if (node.async) {
    this._catchUp("start", key.loc);

    this.word("async");
    this.space();
  }

  if (kind === "method" || kind === "init") {
    if (node.generator) {
      this.token("*");
    }
  }

  if (node.computed) {
    this.token("[");
    this.print(key, node);
    this.token("]");
  } else {
    this.print(key, node);
  }

  if (node.optional) {
    this.token("?");
  }

  this._params(node);
}

function _predicate(node) {
  if (node.predicate) {
    if (!node.returnType) {
      this.token(":");
    }

    this.space();
    this.print(node.predicate, node);
  }
}

function _functionHead(node) {
  if (node.async) {
    this.word("async");
    this.space();
  }

  this.word("function");
  if (node.generator) this.token("*");
  this.printInnerComments(node);
  this.space();

  if (node.id) {
    this.print(node.id, node);
  }

  this._params(node);

  if (node.type !== "TSDeclareFunction") {
    this._predicate(node);
  }
}

function FunctionExpression(node) {
  this._functionHead(node);

  this.space();
  this.print(node.body, node);
}

function ArrowFunctionExpression(node) {
  if (node.async) {
    this.word("async");
    this.space();
  }

  const firstParam = node.params[0];

  if (!this.format.retainLines && !this.format.auxiliaryCommentBefore && !this.format.auxiliaryCommentAfter && node.params.length === 1 && isIdentifier(firstParam) && !hasTypesOrComments(node, firstParam)) {
    this.print(firstParam, node);
  } else {
    this._params(node);
  }

  this._predicate(node);

  this.space();
  this.token("=>");
  this.space();
  this.print(node.body, node);
}

function hasTypesOrComments(node, param) {
  var _param$leadingComment, _param$trailingCommen;

  return !!(node.typeParameters || node.returnType || node.predicate || param.typeAnnotation || param.optional || (_param$leadingComment = param.leadingComments) != null && _param$leadingComment.length || (_param$trailingCommen = param.trailingComments) != null && _param$trailingCommen.length);
}