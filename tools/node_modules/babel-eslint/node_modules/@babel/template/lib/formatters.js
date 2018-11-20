"use strict";

exports.__esModule = true;
exports.program = exports.expression = exports.statement = exports.statements = exports.smart = void 0;

function makeStatementFormatter(fn) {
  return {
    code: function code(str) {
      return "/* @babel/template */;\n" + str;
    },
    validate: function validate() {},
    unwrap: function unwrap(ast) {
      return fn(ast.program.body.slice(1));
    }
  };
}

var smart = makeStatementFormatter(function (body) {
  if (body.length > 1) {
    return body;
  } else {
    return body[0];
  }
});
exports.smart = smart;
var statements = makeStatementFormatter(function (body) {
  return body;
});
exports.statements = statements;
var statement = makeStatementFormatter(function (body) {
  if (body.length === 0) {
    throw new Error("Found nothing to return.");
  }

  if (body.length > 1) {
    throw new Error("Found multiple statements but wanted one");
  }

  return body[0];
});
exports.statement = statement;
var expression = {
  code: function code(str) {
    return "(\n" + str + "\n)";
  },
  validate: function validate(ast) {
    var program = ast.program;

    if (program.body.length > 1) {
      throw new Error("Found multiple statements but wanted one");
    }

    var expression = program.body[0].expression;

    if (expression.start === 0) {
      throw new Error("Parse result included parens.");
    }
  },
  unwrap: function unwrap(ast) {
    return ast.program.body[0].expression;
  }
};
exports.expression = expression;
var program = {
  code: function code(str) {
    return str;
  },
  validate: function validate() {},
  unwrap: function unwrap(ast) {
    return ast.program;
  }
};
exports.program = program;