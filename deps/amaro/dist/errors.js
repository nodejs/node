"use strict";
var __defProp = Object.defineProperty;
var __getOwnPropDesc = Object.getOwnPropertyDescriptor;
var __getOwnPropNames = Object.getOwnPropertyNames;
var __hasOwnProp = Object.prototype.hasOwnProperty;
var __export = (target, all) => {
  for (var name in all)
    __defProp(target, name, { get: all[name], enumerable: true });
};
var __copyProps = (to, from, except, desc) => {
  if (from && typeof from === "object" || typeof from === "function") {
    for (let key of __getOwnPropNames(from))
      if (!__hasOwnProp.call(to, key) && key !== except)
        __defProp(to, key, { get: () => from[key], enumerable: !(desc = __getOwnPropDesc(from, key)) || desc.enumerable });
  }
  return to;
};
var __toCommonJS = (mod) => __copyProps(__defProp({}, "__esModule", { value: true }), mod);
var errors_exports = {};
__export(errors_exports, {
  isSwcError: () => isSwcError,
  wrapAndReThrowSwcError: () => wrapAndReThrowSwcError
});
module.exports = __toCommonJS(errors_exports);
function isSwcError(error) {
  return error.code !== void 0;
}
function wrapAndReThrowSwcError(error) {
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
// Annotate the CommonJS export names for ESM import in node:
0 && (module.exports = {
  isSwcError,
  wrapAndReThrowSwcError
});
