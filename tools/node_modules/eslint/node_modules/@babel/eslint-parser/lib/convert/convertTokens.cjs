const ESLINT_VERSION = require("../utils/eslint-version.cjs");

function convertTemplateType(tokens, tl) {
  let curlyBrace = null;
  let templateTokens = [];
  const result = [];

  function addTemplateType() {
    const start = templateTokens[0];
    const end = templateTokens[templateTokens.length - 1];
    const value = templateTokens.reduce((result, token) => {
      if (token.value) {
        result += token.value;
      } else if (token.type.label !== tl.template) {
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
    switch (token.type.label) {
      case tl.backQuote:
        if (curlyBrace) {
          result.push(curlyBrace);
          curlyBrace = null;
        }

        templateTokens.push(token);

        if (templateTokens.length > 1) {
          addTemplateType();
        }

        break;

      case tl.dollarBraceL:
        templateTokens.push(token);
        addTemplateType();
        break;

      case tl.braceR:
        if (curlyBrace) {
          result.push(curlyBrace);
        }

        curlyBrace = token;
        break;

      case tl.template:
        if (curlyBrace) {
          templateTokens.push(curlyBrace);
          curlyBrace = null;
        }

        templateTokens.push(token);
        break;

      case tl.eof:
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

function convertToken(token, source, tl) {
  const {
    type
  } = token;
  const {
    label
  } = type;
  token.range = [token.start, token.end];

  if (label === tl.name) {
    if (token.value === "static") {
      token.type = "Keyword";
    } else {
      token.type = "Identifier";
    }
  } else if (label === tl.semi || label === tl.comma || label === tl.parenL || label === tl.parenR || label === tl.braceL || label === tl.braceR || label === tl.slash || label === tl.dot || label === tl.bracketL || label === tl.bracketR || label === tl.ellipsis || label === tl.arrow || label === tl.pipeline || label === tl.star || label === tl.incDec || label === tl.colon || label === tl.question || label === tl.template || label === tl.backQuote || label === tl.dollarBraceL || label === tl.at || label === tl.logicalOR || label === tl.logicalAND || label === tl.nullishCoalescing || label === tl.bitwiseOR || label === tl.bitwiseXOR || label === tl.bitwiseAND || label === tl.equality || label === tl.relational || label === tl.bitShift || label === tl.plusMin || label === tl.modulo || label === tl.exponent || label === tl.bang || label === tl.tilde || label === tl.doubleColon || label === tl.hash || label === tl.questionDot || label === tl.braceHashL || label === tl.braceBarL || label === tl.braceBarR || label === tl.bracketHashL || label === tl.bracketBarL || label === tl.bracketBarR || type.isAssign) {
    var _token$value;

    token.type = "Punctuator";
    (_token$value = token.value) != null ? _token$value : token.value = label;
  } else if (label === tl.jsxTagStart) {
    token.type = "Punctuator";
    token.value = "<";
  } else if (label === tl.jsxTagEnd) {
    token.type = "Punctuator";
    token.value = ">";
  } else if (label === tl.jsxName) {
    token.type = "JSXIdentifier";
  } else if (label === tl.jsxText) {
    token.type = "JSXText";
  } else if (type.keyword === "null") {
    token.type = "Null";
  } else if (type.keyword === "false" || type.keyword === "true") {
    token.type = "Boolean";
  } else if (type.keyword) {
    token.type = "Keyword";
  } else if (label === tl.num) {
    token.type = "Numeric";
    token.value = source.slice(token.start, token.end);
  } else if (label === tl.string) {
    token.type = "String";
    token.value = source.slice(token.start, token.end);
  } else if (label === tl.regexp) {
    token.type = "RegularExpression";
    const value = token.value;
    token.regex = {
      pattern: value.pattern,
      flags: value.flags
    };
    token.value = `/${value.pattern}/${value.flags}`;
  } else if (label === tl.bigint) {
    token.type = "Numeric";
    token.value = `${token.value}n`;
  } else if (label === tl.privateName) {
    token.type = "PrivateIdentifier";
  }

  if (typeof token.type !== "string") {
    delete token.type.rightAssociative;
  }
}

module.exports = function convertTokens(tokens, code, tl) {
  const result = [];
  const withoutComments = convertTemplateType(tokens, tl).filter(t => t.type !== "CommentLine" && t.type !== "CommentBlock");

  for (let i = 0, {
    length
  } = withoutComments; i < length; i++) {
    const token = withoutComments[i];
    {
      if (ESLINT_VERSION >= 8 && i + 1 < length && token.type.label === tl.hash) {
        const nextToken = withoutComments[i + 1];

        if (nextToken.type.label === tl.name && token.end === nextToken.start) {
          i++;
          nextToken.type = "PrivateIdentifier";
          nextToken.start -= 1;
          nextToken.loc.start.column -= 1;
          nextToken.range = [nextToken.start, nextToken.end];
          result.push(nextToken);
          continue;
        }
      }
    }
    convertToken(token, code, tl);
    result.push(token);
  }

  return result;
};