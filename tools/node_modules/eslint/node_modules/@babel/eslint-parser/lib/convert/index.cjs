const convertTokens = require("./convertTokens.cjs");

const convertComments = require("./convertComments.cjs");

const convertAST = require("./convertAST.cjs");

exports.ast = function convert(ast, code, tokLabels, visitorKeys) {
  ast.tokens = convertTokens(ast.tokens, code, tokLabels);
  convertComments(ast.comments);
  convertAST(ast, visitorKeys);
  return ast;
};

exports.error = function convertError(err) {
  if (err instanceof SyntaxError) {
    err.lineNumber = err.loc.line;
    err.column = err.loc.column;
  }

  return err;
};