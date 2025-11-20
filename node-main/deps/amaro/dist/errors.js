"use strict";
export function isSwcError(error) {
  return error.code !== void 0;
}
export function wrapAndReThrowSwcError(error) {
  const errorHints = `${error.filename}:${error.startLine}
${error.snippet}
`;
  switch (error.code) {
    case "UnsupportedSyntax": {
      const unsupportedSyntaxError = new Error(error.message);
      unsupportedSyntaxError.name = "UnsupportedSyntaxError";
      unsupportedSyntaxError.stack = `${errorHints}${unsupportedSyntaxError.stack}`;
      throw unsupportedSyntaxError;
    }
    case "InvalidSyntax": {
      const syntaxError = new SyntaxError(error.message);
      syntaxError.stack = `${errorHints}${syntaxError.stack}`;
      throw syntaxError;
    }
    default:
      throw new Error(error.message);
  }
}
