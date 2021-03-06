"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = parseAndBuildMetadata;

var t = _interopRequireWildcard(require("@babel/types"));

var _parser = require("@babel/parser");

var _codeFrame = require("@babel/code-frame");

function _getRequireWildcardCache() { if (typeof WeakMap !== "function") return null; var cache = new WeakMap(); _getRequireWildcardCache = function () { return cache; }; return cache; }

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } if (obj === null || typeof obj !== "object" && typeof obj !== "function") { return { default: obj }; } var cache = _getRequireWildcardCache(); if (cache && cache.has(obj)) { return cache.get(obj); } var newObj = {}; var hasPropertyDescriptor = Object.defineProperty && Object.getOwnPropertyDescriptor; for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = hasPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : null; if (desc && (desc.get || desc.set)) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } newObj.default = obj; if (cache) { cache.set(obj, newObj); } return newObj; }

const PATTERN = /^[_$A-Z0-9]+$/;

function parseAndBuildMetadata(formatter, code, opts) {
  const {
    placeholderWhitelist,
    placeholderPattern,
    preserveComments,
    syntacticPlaceholders
  } = opts;
  const ast = parseWithCodeFrame(code, opts.parser, syntacticPlaceholders);
  t.removePropertiesDeep(ast, {
    preserveComments
  });
  formatter.validate(ast);
  const syntactic = {
    placeholders: [],
    placeholderNames: new Set()
  };
  const legacy = {
    placeholders: [],
    placeholderNames: new Set()
  };
  const isLegacyRef = {
    value: undefined
  };
  t.traverse(ast, placeholderVisitorHandler, {
    syntactic,
    legacy,
    isLegacyRef,
    placeholderWhitelist,
    placeholderPattern,
    syntacticPlaceholders
  });
  return Object.assign({
    ast
  }, isLegacyRef.value ? legacy : syntactic);
}

function placeholderVisitorHandler(node, ancestors, state) {
  var _state$placeholderWhi;

  let name;

  if (t.isPlaceholder(node)) {
    if (state.syntacticPlaceholders === false) {
      throw new Error("%%foo%%-style placeholders can't be used when " + "'.syntacticPlaceholders' is false.");
    } else {
      name = node.name.name;
      state.isLegacyRef.value = false;
    }
  } else if (state.isLegacyRef.value === false || state.syntacticPlaceholders) {
    return;
  } else if (t.isIdentifier(node) || t.isJSXIdentifier(node)) {
    name = node.name;
    state.isLegacyRef.value = true;
  } else if (t.isStringLiteral(node)) {
    name = node.value;
    state.isLegacyRef.value = true;
  } else {
    return;
  }

  if (!state.isLegacyRef.value && (state.placeholderPattern != null || state.placeholderWhitelist != null)) {
    throw new Error("'.placeholderWhitelist' and '.placeholderPattern' aren't compatible" + " with '.syntacticPlaceholders: true'");
  }

  if (state.isLegacyRef.value && (state.placeholderPattern === false || !(state.placeholderPattern || PATTERN).test(name)) && !((_state$placeholderWhi = state.placeholderWhitelist) == null ? void 0 : _state$placeholderWhi.has(name))) {
    return;
  }

  ancestors = ancestors.slice();
  const {
    node: parent,
    key
  } = ancestors[ancestors.length - 1];
  let type;

  if (t.isStringLiteral(node) || t.isPlaceholder(node, {
    expectedNode: "StringLiteral"
  })) {
    type = "string";
  } else if (t.isNewExpression(parent) && key === "arguments" || t.isCallExpression(parent) && key === "arguments" || t.isFunction(parent) && key === "params") {
    type = "param";
  } else if (t.isExpressionStatement(parent) && !t.isPlaceholder(node)) {
    type = "statement";
    ancestors = ancestors.slice(0, -1);
  } else if (t.isStatement(node) && t.isPlaceholder(node)) {
    type = "statement";
  } else {
    type = "other";
  }

  const {
    placeholders,
    placeholderNames
  } = state.isLegacyRef.value ? state.legacy : state.syntactic;
  placeholders.push({
    name,
    type,
    resolve: ast => resolveAncestors(ast, ancestors),
    isDuplicate: placeholderNames.has(name)
  });
  placeholderNames.add(name);
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

function parseWithCodeFrame(code, parserOpts, syntacticPlaceholders) {
  const plugins = (parserOpts.plugins || []).slice();

  if (syntacticPlaceholders !== false) {
    plugins.push("placeholders");
  }

  parserOpts = Object.assign({
    allowReturnOutsideFunction: true,
    allowSuperOutsideMethod: true,
    sourceType: "module"
  }, parserOpts, {
    plugins
  });

  try {
    return (0, _parser.parse)(code, parserOpts);
  } catch (err) {
    const loc = err.loc;

    if (loc) {
      err.message += "\n" + (0, _codeFrame.codeFrameColumns)(code, {
        start: loc
      });
      err.code = "BABEL_TEMPLATE_PARSE_ERROR";
    }

    throw err;
  }
}