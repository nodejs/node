"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.ClassAccessorProperty = ClassAccessorProperty;
exports.ClassBody = ClassBody;
exports.ClassExpression = exports.ClassDeclaration = ClassDeclaration;
exports.ClassMethod = ClassMethod;
exports.ClassPrivateMethod = ClassPrivateMethod;
exports.ClassPrivateProperty = ClassPrivateProperty;
exports.ClassProperty = ClassProperty;
exports.StaticBlock = StaticBlock;
exports._classMethodHead = _classMethodHead;

var _t = require("@babel/types");

const {
  isExportDefaultDeclaration,
  isExportNamedDeclaration
} = _t;

function ClassDeclaration(node, parent) {
  {
    if (!this.format.decoratorsBeforeExport || !isExportDefaultDeclaration(parent) && !isExportNamedDeclaration(parent)) {
      this.printJoin(node.decorators, node);
    }
  }

  if (node.declare) {
    this.word("declare");
    this.space();
  }

  if (node.abstract) {
    this.word("abstract");
    this.space();
  }

  this.word("class");
  this.printInnerComments(node);

  if (node.id) {
    this.space();
    this.print(node.id, node);
  }

  this.print(node.typeParameters, node);

  if (node.superClass) {
    this.space();
    this.word("extends");
    this.space();
    this.print(node.superClass, node);
    this.print(node.superTypeParameters, node);
  }

  if (node.implements) {
    this.space();
    this.word("implements");
    this.space();
    this.printList(node.implements, node);
  }

  this.space();
  this.print(node.body, node);
}

function ClassBody(node) {
  this.tokenChar(123);
  this.printInnerComments(node);

  if (node.body.length === 0) {
    this.tokenChar(125);
  } else {
    this.newline();
    this.indent();
    this.printSequence(node.body, node);
    this.dedent();
    if (!this.endsWith(10)) this.newline();
    this.rightBrace();
  }
}

function ClassProperty(node) {
  this.printJoin(node.decorators, node);
  this.source("end", node.key.loc);
  this.tsPrintClassMemberModifiers(node);

  if (node.computed) {
    this.tokenChar(91);
    this.print(node.key, node);
    this.tokenChar(93);
  } else {
    this._variance(node);

    this.print(node.key, node);
  }

  if (node.optional) {
    this.tokenChar(63);
  }

  if (node.definite) {
    this.tokenChar(33);
  }

  this.print(node.typeAnnotation, node);

  if (node.value) {
    this.space();
    this.tokenChar(61);
    this.space();
    this.print(node.value, node);
  }

  this.semicolon();
}

function ClassAccessorProperty(node) {
  this.printJoin(node.decorators, node);
  this.source("end", node.key.loc);
  this.tsPrintClassMemberModifiers(node);
  this.word("accessor");
  this.printInnerComments(node);
  this.space();

  if (node.computed) {
    this.tokenChar(91);
    this.print(node.key, node);
    this.tokenChar(93);
  } else {
    this._variance(node);

    this.print(node.key, node);
  }

  if (node.optional) {
    this.tokenChar(63);
  }

  if (node.definite) {
    this.tokenChar(33);
  }

  this.print(node.typeAnnotation, node);

  if (node.value) {
    this.space();
    this.tokenChar(61);
    this.space();
    this.print(node.value, node);
  }

  this.semicolon();
}

function ClassPrivateProperty(node) {
  this.printJoin(node.decorators, node);

  if (node.static) {
    this.word("static");
    this.space();
  }

  this.print(node.key, node);
  this.print(node.typeAnnotation, node);

  if (node.value) {
    this.space();
    this.tokenChar(61);
    this.space();
    this.print(node.value, node);
  }

  this.semicolon();
}

function ClassMethod(node) {
  this._classMethodHead(node);

  this.space();
  this.print(node.body, node);
}

function ClassPrivateMethod(node) {
  this._classMethodHead(node);

  this.space();
  this.print(node.body, node);
}

function _classMethodHead(node) {
  this.printJoin(node.decorators, node);
  this.source("end", node.key.loc);
  this.tsPrintClassMemberModifiers(node);

  this._methodHead(node);
}

function StaticBlock(node) {
  this.word("static");
  this.space();
  this.tokenChar(123);

  if (node.body.length === 0) {
    this.tokenChar(125);
  } else {
    this.newline();
    this.printSequence(node.body, node, {
      indent: true
    });
    this.rightBrace();
  }
}