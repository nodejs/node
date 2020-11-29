"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = convertTokens;

var _core = require("@babel/core");

function convertTemplateType(tokens) {
  let curlyBrace = null;
  let templateTokens = [];
  const result = [];

  function addTemplateType() {
    const start = templateTokens[0];
    const end = templateTokens[templateTokens.length - 1];
    const value = templateTokens.reduce((result, token) => {
      if (token.value) {
        result += token.value;
      } else if (token.type !== _core.tokTypes.template) {
        result += token.type.label;
      }

      return result;
    }, "");
    result.push({
      type: "Template",
      value: value,
      start: start.start,
      end: end.end,
      loc: {
        start: start.loc.start,
        end: end.loc.end
      }
    });
    templateTokens = [];
  }

  tokens.forEach(token => {
    switch (token.type) {
      case _core.tokTypes.backQuote:
        if (curlyBrace) {
          result.push(curlyBrace);
          curlyBrace = null;
        }

        templateTokens.push(token);

        if (templateTokens.length > 1) {
          addTemplateType();
        }

        break;

      case _core.tokTypes.dollarBraceL:
        templateTokens.push(token);
        addTemplateType();
        break;

      case _core.tokTypes.braceR:
        if (curlyBrace) {
          result.push(curlyBrace);
        }

        curlyBrace = token;
        break;

      case _core.tokTypes.template:
        if (curlyBrace) {
          templateTokens.push(curlyBrace);
          curlyBrace = null;
        }

        templateTokens.push(token);
        break;

      case _core.tokTypes.eof:
        if (curlyBrace) {
          result.push(curlyBrace);
        }

        break;

      default:
        if (curlyBrace) {
          result.push(curlyBrace);
          curlyBrace = null;
        }

        result.push(token);
    }
  });
  return result;
}

function convertToken(token, source) {
  const type = token.type;
  token.range = [token.start, token.end];

  if (type === _core.tokTypes.name) {
    token.type = "Identifier";
  } else if (type === _core.tokTypes.semi || type === _core.tokTypes.comma || type === _core.tokTypes.parenL || type === _core.tokTypes.parenR || type === _core.tokTypes.braceL || type === _core.tokTypes.braceR || type === _core.tokTypes.slash || type === _core.tokTypes.dot || type === _core.tokTypes.bracketL || type === _core.tokTypes.bracketR || type === _core.tokTypes.ellipsis || type === _core.tokTypes.arrow || type === _core.tokTypes.pipeline || type === _core.tokTypes.star || type === _core.tokTypes.incDec || type === _core.tokTypes.colon || type === _core.tokTypes.question || type === _core.tokTypes.template || type === _core.tokTypes.backQuote || type === _core.tokTypes.dollarBraceL || type === _core.tokTypes.at || type === _core.tokTypes.logicalOR || type === _core.tokTypes.logicalAND || type === _core.tokTypes.nullishCoalescing || type === _core.tokTypes.bitwiseOR || type === _core.tokTypes.bitwiseXOR || type === _core.tokTypes.bitwiseAND || type === _core.tokTypes.equality || type === _core.tokTypes.relational || type === _core.tokTypes.bitShift || type === _core.tokTypes.plusMin || type === _core.tokTypes.modulo || type === _core.tokTypes.exponent || type === _core.tokTypes.bang || type === _core.tokTypes.tilde || type === _core.tokTypes.doubleColon || type === _core.tokTypes.hash || type === _core.tokTypes.questionDot || type.isAssign) {
    token.type = "Punctuator";
    if (!token.value) token.value = type.label;
  } else if (type === _core.tokTypes.jsxTagStart) {
    token.type = "Punctuator";
    token.value = "<";
  } else if (type === _core.tokTypes.jsxTagEnd) {
    token.type = "Punctuator";
    token.value = ">";
  } else if (type === _core.tokTypes.jsxName) {
    token.type = "JSXIdentifier";
  } else if (type === _core.tokTypes.jsxText) {
    token.type = "JSXText";
  } else if (type.keyword === "null") {
    token.type = "Null";
  } else if (type.keyword === "false" || type.keyword === "true") {
    token.type = "Boolean";
  } else if (type.keyword) {
    token.type = "Keyword";
  } else if (type === _core.tokTypes.num) {
    token.type = "Numeric";
    token.value = source.slice(token.start, token.end);
  } else if (type === _core.tokTypes.string) {
    token.type = "String";
    token.value = source.slice(token.start, token.end);
  } else if (type === _core.tokTypes.regexp) {
    token.type = "RegularExpression";
    const value = token.value;
    token.regex = {
      pattern: value.pattern,
      flags: value.flags
    };
    token.value = `/${value.pattern}/${value.flags}`;
  } else if (type === _core.tokTypes.bigint) {
    token.type = "Numeric";
    token.value = `${token.value}n`;
  }

  if (typeof token.type !== "string") {
    delete token.type.rightAssociative;
  }

  return token;
}

function convertTokens(tokens, code) {
  return convertTemplateType(tokens).filter(t => t.type !== "CommentLine" && t.type !== "CommentBlock").map(t => convertToken(t, code));
}