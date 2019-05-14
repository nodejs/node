"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = parseAndBuildMetadata;

function t() {
  const data = _interopRequireWildcard(require("@babel/types"));

  t = function () {
    return data;
  };

  return data;
}

function _parser() {
  const data = require("@babel/parser");

  _parser = function () {
    return data;
  };

  return data;
}

function _codeFrame() {
  const data = require("@babel/code-frame");

  _codeFrame = function () {
    return data;
  };

  return data;
}

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

const PATTERN = /^[_$A-Z0-9]+$/;

function parseAndBuildMetadata(formatter, code, opts) {
  const ast = parseWithCodeFrame(code, opts.parser);
  const {
    placeholderWhitelist,
    placeholderPattern = PATTERN,
    preserveComments
  } = opts;
  t().removePropertiesDeep(ast, {
    preserveComments
  });
  formatter.validate(ast);
  const placeholders = [];
  const placeholderNames = new Set();
  t().traverse(ast, placeholderVisitorHandler, {
    placeholders,
    placeholderNames,
    placeholderWhitelist,
    placeholderPattern
  });
  return {
    ast,
    placeholders,
    placeholderNames
  };
}

function placeholderVisitorHandler(node, ancestors, state) {
  let name;

  if (t().isIdentifier(node) || t().isJSXIdentifier(node)) {
    name = node.name;
  } else if (t().isStringLiteral(node)) {
    name = node.value;
  } else {
    return;
  }

  if ((!state.placeholderPattern || !state.placeholderPattern.test(name)) && (!state.placeholderWhitelist || !state.placeholderWhitelist.has(name))) {
    return;
  }

  ancestors = ancestors.slice();
  const {
    node: parent,
    key
  } = ancestors[ancestors.length - 1];
  let type;

  if (t().isStringLiteral(node)) {
    type = "string";
  } else if (t().isNewExpression(parent) && key === "arguments" || t().isCallExpression(parent) && key === "arguments" || t().isFunction(parent) && key === "params") {
    type = "param";
  } else if (t().isExpressionStatement(parent)) {
    type = "statement";
    ancestors = ancestors.slice(0, -1);
  } else {
    type = "other";
  }

  state.placeholders.push({
    name,
    type,
    resolve: ast => resolveAncestors(ast, ancestors),
    isDuplicate: state.placeholderNames.has(name)
  });
  state.placeholderNames.add(name);
}

function resolveAncestors(ast, ancestors) {
  let parent = ast;

  for (let i = 0; i < ancestors.length - 1; i++) {
    const {
      key,
      index
    } = ancestors[i];

    if (index === undefined) {
      parent = parent[key];
    } else {
      parent = parent[key][index];
    }
  }

  const {
    key,
    index
  } = ancestors[ancestors.length - 1];
  return {
    parent,
    key,
    index
  };
}

function parseWithCodeFrame(code, parserOpts) {
  parserOpts = Object.assign({
    allowReturnOutsideFunction: true,
    allowSuperOutsideMethod: true,
    sourceType: "module"
  }, parserOpts);

  try {
    return (0, _parser().parse)(code, parserOpts);
  } catch (err) {
    const loc = err.loc;

    if (loc) {
      err.message += "\n" + (0, _codeFrame().codeFrameColumns)(code, {
        start: loc
      });
      err.code = "BABEL_TEMPLATE_PARSE_ERROR";
    }

    throw err;
  }
}