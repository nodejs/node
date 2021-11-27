"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.LogicalExpression = exports.BinaryExpression = exports.AssignmentExpression = AssignmentExpression;
exports.AssignmentPattern = AssignmentPattern;
exports.AwaitExpression = void 0;
exports.BindExpression = BindExpression;
exports.CallExpression = CallExpression;
exports.ConditionalExpression = ConditionalExpression;
exports.Decorator = Decorator;
exports.DoExpression = DoExpression;
exports.EmptyStatement = EmptyStatement;
exports.ExpressionStatement = ExpressionStatement;
exports.Import = Import;
exports.MemberExpression = MemberExpression;
exports.MetaProperty = MetaProperty;
exports.ModuleExpression = ModuleExpression;
exports.NewExpression = NewExpression;
exports.OptionalCallExpression = OptionalCallExpression;
exports.OptionalMemberExpression = OptionalMemberExpression;
exports.ParenthesizedExpression = ParenthesizedExpression;
exports.PrivateName = PrivateName;
exports.SequenceExpression = SequenceExpression;
exports.Super = Super;
exports.ThisExpression = ThisExpression;
exports.UnaryExpression = UnaryExpression;
exports.UpdateExpression = UpdateExpression;
exports.V8IntrinsicIdentifier = V8IntrinsicIdentifier;
exports.YieldExpression = void 0;

var _t = require("@babel/types");

var n = require("../node");

const {
  isCallExpression,
  isLiteral,
  isMemberExpression,
  isNewExpression
} = _t;

function UnaryExpression(node) {
  if (node.operator === "void" || node.operator === "delete" || node.operator === "typeof" || node.operator === "throw") {
    this.word(node.operator);
    this.space();
  } else {
    this.token(node.operator);
  }

  this.print(node.argument, node);
}

function DoExpression(node) {
  if (node.async) {
    this.word("async");
    this.space();
  }

  this.word("do");
  this.space();
  this.print(node.body, node);
}

function ParenthesizedExpression(node) {
  this.token("(");
  this.print(node.expression, node);
  this.token(")");
}

function UpdateExpression(node) {
  if (node.prefix) {
    this.token(node.operator);
    this.print(node.argument, node);
  } else {
    this.startTerminatorless(true);
    this.print(node.argument, node);
    this.endTerminatorless();
    this.token(node.operator);
  }
}

function ConditionalExpression(node) {
  this.print(node.test, node);
  this.space();
  this.token("?");
  this.space();
  this.print(node.consequent, node);
  this.space();
  this.token(":");
  this.space();
  this.print(node.alternate, node);
}

function NewExpression(node, parent) {
  this.word("new");
  this.space();
  this.print(node.callee, node);

  if (this.format.minified && node.arguments.length === 0 && !node.optional && !isCallExpression(parent, {
    callee: node
  }) && !isMemberExpression(parent) && !isNewExpression(parent)) {
    return;
  }

  this.print(node.typeArguments, node);
  this.print(node.typeParameters, node);

  if (node.optional) {
    this.token("?.");
  }

  this.token("(");
  this.printList(node.arguments, node);
  this.token(")");
}

function SequenceExpression(node) {
  this.printList(node.expressions, node);
}

function ThisExpression() {
  this.word("this");
}

function Super() {
  this.word("super");
}

function Decorator(node) {
  this.token("@");
  this.print(node.expression, node);
  this.newline();
}

function OptionalMemberExpression(node) {
  this.print(node.object, node);

  if (!node.computed && isMemberExpression(node.property)) {
    throw new TypeError("Got a MemberExpression for MemberExpression property");
  }

  let computed = node.computed;

  if (isLiteral(node.property) && typeof node.property.value === "number") {
    computed = true;
  }

  if (node.optional) {
    this.token("?.");
  }

  if (computed) {
    this.token("[");
    this.print(node.property, node);
    this.token("]");
  } else {
    if (!node.optional) {
      this.token(".");
    }

    this.print(node.property, node);
  }
}

function OptionalCallExpression(node) {
  this.print(node.callee, node);
  this.print(node.typeArguments, node);
  this.print(node.typeParameters, node);

  if (node.optional) {
    this.token("?.");
  }

  this.token("(");
  this.printList(node.arguments, node);
  this.token(")");
}

function CallExpression(node) {
  this.print(node.callee, node);
  this.print(node.typeArguments, node);
  this.print(node.typeParameters, node);
  this.token("(");
  this.printList(node.arguments, node);
  this.token(")");
}

function Import() {
  this.word("import");
}

function buildYieldAwait(keyword) {
  return function (node) {
    this.word(keyword);

    if (node.delegate) {
      this.token("*");
    }

    if (node.argument) {
      this.space();
      const terminatorState = this.startTerminatorless();
      this.print(node.argument, node);
      this.endTerminatorless(terminatorState);
    }
  };
}

const YieldExpression = buildYieldAwait("yield");
exports.YieldExpression = YieldExpression;
const AwaitExpression = buildYieldAwait("await");
exports.AwaitExpression = AwaitExpression;

function EmptyStatement() {
  this.semicolon(true);
}

function ExpressionStatement(node) {
  this.print(node.expression, node);
  this.semicolon();
}

function AssignmentPattern(node) {
  this.print(node.left, node);
  if (node.left.optional) this.token("?");
  this.print(node.left.typeAnnotation, node);
  this.space();
  this.token("=");
  this.space();
  this.print(node.right, node);
}

function AssignmentExpression(node, parent) {
  const parens = this.inForStatementInitCounter && node.operator === "in" && !n.needsParens(node, parent);

  if (parens) {
    this.token("(");
  }

  this.print(node.left, node);
  this.space();

  if (node.operator === "in" || node.operator === "instanceof") {
    this.word(node.operator);
  } else {
    this.token(node.operator);
  }

  this.space();
  this.print(node.right, node);

  if (parens) {
    this.token(")");
  }
}

function BindExpression(node) {
  this.print(node.object, node);
  this.token("::");
  this.print(node.callee, node);
}

function MemberExpression(node) {
  this.print(node.object, node);

  if (!node.computed && isMemberExpression(node.property)) {
    throw new TypeError("Got a MemberExpression for MemberExpression property");
  }

  let computed = node.computed;

  if (isLiteral(node.property) && typeof node.property.value === "number") {
    computed = true;
  }

  if (computed) {
    this.token("[");
    this.print(node.property, node);
    this.token("]");
  } else {
    this.token(".");
    this.print(node.property, node);
  }
}

function MetaProperty(node) {
  this.print(node.meta, node);
  this.token(".");
  this.print(node.property, node);
}

function PrivateName(node) {
  this.token("#");
  this.print(node.id, node);
}

function V8IntrinsicIdentifier(node) {
  this.token("%");
  this.word(node.name);
}

function ModuleExpression(node) {
  this.word("module");
  this.space();
  this.token("{");

  if (node.body.body.length === 0) {
    this.token("}");
  } else {
    this.newline();
    this.printSequence(node.body.body, node, {
      indent: true
    });
    this.rightBrace();
  }
}