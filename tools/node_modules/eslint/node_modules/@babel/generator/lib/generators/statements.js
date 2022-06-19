"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.BreakStatement = void 0;
exports.CatchClause = CatchClause;
exports.ContinueStatement = void 0;
exports.DebuggerStatement = DebuggerStatement;
exports.DoWhileStatement = DoWhileStatement;
exports.ForOfStatement = exports.ForInStatement = void 0;
exports.ForStatement = ForStatement;
exports.IfStatement = IfStatement;
exports.LabeledStatement = LabeledStatement;
exports.ReturnStatement = void 0;
exports.SwitchCase = SwitchCase;
exports.SwitchStatement = SwitchStatement;
exports.ThrowStatement = void 0;
exports.TryStatement = TryStatement;
exports.VariableDeclaration = VariableDeclaration;
exports.VariableDeclarator = VariableDeclarator;
exports.WhileStatement = WhileStatement;
exports.WithStatement = WithStatement;

var _t = require("@babel/types");

const {
  isFor,
  isForStatement,
  isIfStatement,
  isStatement
} = _t;

function WithStatement(node) {
  this.word("with");
  this.space();
  this.token("(");
  this.print(node.object, node);
  this.token(")");
  this.printBlock(node);
}

function IfStatement(node) {
  this.word("if");
  this.space();
  this.token("(");
  this.print(node.test, node);
  this.token(")");
  this.space();
  const needsBlock = node.alternate && isIfStatement(getLastStatement(node.consequent));

  if (needsBlock) {
    this.token("{");
    this.newline();
    this.indent();
  }

  this.printAndIndentOnComments(node.consequent, node);

  if (needsBlock) {
    this.dedent();
    this.newline();
    this.token("}");
  }

  if (node.alternate) {
    if (this.endsWith(125)) this.space();
    this.word("else");
    this.space();
    this.printAndIndentOnComments(node.alternate, node);
  }
}

function getLastStatement(statement) {
  if (!isStatement(statement.body)) return statement;
  return getLastStatement(statement.body);
}

function ForStatement(node) {
  this.word("for");
  this.space();
  this.token("(");
  this.inForStatementInitCounter++;
  this.print(node.init, node);
  this.inForStatementInitCounter--;
  this.token(";");

  if (node.test) {
    this.space();
    this.print(node.test, node);
  }

  this.token(";");

  if (node.update) {
    this.space();
    this.print(node.update, node);
  }

  this.token(")");
  this.printBlock(node);
}

function WhileStatement(node) {
  this.word("while");
  this.space();
  this.token("(");
  this.print(node.test, node);
  this.token(")");
  this.printBlock(node);
}

const buildForXStatement = function (op) {
  return function (node) {
    this.word("for");
    this.space();

    if (op === "of" && node.await) {
      this.word("await");
      this.space();
    }

    this.token("(");
    this.print(node.left, node);
    this.space();
    this.word(op);
    this.space();
    this.print(node.right, node);
    this.token(")");
    this.printBlock(node);
  };
};

const ForInStatement = buildForXStatement("in");
exports.ForInStatement = ForInStatement;
const ForOfStatement = buildForXStatement("of");
exports.ForOfStatement = ForOfStatement;

function DoWhileStatement(node) {
  this.word("do");
  this.space();
  this.print(node.body, node);
  this.space();
  this.word("while");
  this.space();
  this.token("(");
  this.print(node.test, node);
  this.token(")");
  this.semicolon();
}

function buildLabelStatement(prefix, key = "label") {
  return function (node) {
    this.word(prefix);
    const label = node[key];

    if (label) {
      this.space();
      const isLabel = key == "label";
      const terminatorState = this.startTerminatorless(isLabel);
      this.print(label, node);
      this.endTerminatorless(terminatorState);
    }

    this.semicolon();
  };
}

const ContinueStatement = buildLabelStatement("continue");
exports.ContinueStatement = ContinueStatement;
const ReturnStatement = buildLabelStatement("return", "argument");
exports.ReturnStatement = ReturnStatement;
const BreakStatement = buildLabelStatement("break");
exports.BreakStatement = BreakStatement;
const ThrowStatement = buildLabelStatement("throw", "argument");
exports.ThrowStatement = ThrowStatement;

function LabeledStatement(node) {
  this.print(node.label, node);
  this.token(":");
  this.space();
  this.print(node.body, node);
}

function TryStatement(node) {
  this.word("try");
  this.space();
  this.print(node.block, node);
  this.space();

  if (node.handlers) {
    this.print(node.handlers[0], node);
  } else {
    this.print(node.handler, node);
  }

  if (node.finalizer) {
    this.space();
    this.word("finally");
    this.space();
    this.print(node.finalizer, node);
  }
}

function CatchClause(node) {
  this.word("catch");
  this.space();

  if (node.param) {
    this.token("(");
    this.print(node.param, node);
    this.print(node.param.typeAnnotation, node);
    this.token(")");
    this.space();
  }

  this.print(node.body, node);
}

function SwitchStatement(node) {
  this.word("switch");
  this.space();
  this.token("(");
  this.print(node.discriminant, node);
  this.token(")");
  this.space();
  this.token("{");
  this.printSequence(node.cases, node, {
    indent: true,

    addNewlines(leading, cas) {
      if (!leading && node.cases[node.cases.length - 1] === cas) return -1;
    }

  });
  this.token("}");
}

function SwitchCase(node) {
  if (node.test) {
    this.word("case");
    this.space();
    this.print(node.test, node);
    this.token(":");
  } else {
    this.word("default");
    this.token(":");
  }

  if (node.consequent.length) {
    this.newline();
    this.printSequence(node.consequent, node, {
      indent: true
    });
  }
}

function DebuggerStatement() {
  this.word("debugger");
  this.semicolon();
}

function variableDeclarationIndent() {
  this.token(",");
  this.newline();

  if (this.endsWith(10)) {
    for (let i = 0; i < 4; i++) this.space(true);
  }
}

function constDeclarationIndent() {
  this.token(",");
  this.newline();

  if (this.endsWith(10)) {
    for (let i = 0; i < 6; i++) this.space(true);
  }
}

function VariableDeclaration(node, parent) {
  if (node.declare) {
    this.word("declare");
    this.space();
  }

  this.word(node.kind);
  this.space();
  let hasInits = false;

  if (!isFor(parent)) {
    for (const declar of node.declarations) {
      if (declar.init) {
        hasInits = true;
      }
    }
  }

  let separator;

  if (hasInits) {
    separator = node.kind === "const" ? constDeclarationIndent : variableDeclarationIndent;
  }

  this.printList(node.declarations, node, {
    separator
  });

  if (isFor(parent)) {
    if (isForStatement(parent)) {
      if (parent.init === node) return;
    } else {
      if (parent.left === node) return;
    }
  }

  this.semicolon();
}

function VariableDeclarator(node) {
  this.print(node.id, node);
  if (node.definite) this.token("!");
  this.print(node.id.typeAnnotation, node);

  if (node.init) {
    this.space();
    this.token("=");
    this.space();
    this.print(node.init, node);
  }
}