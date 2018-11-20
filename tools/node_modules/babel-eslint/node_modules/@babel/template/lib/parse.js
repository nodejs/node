"use strict";

exports.__esModule = true;
exports.default = parseAndBuildMetadata;

var t = _interopRequireWildcard(require("@babel/types"));

var _babylon = require("babylon");

var _codeFrame = require("@babel/code-frame");

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

var PATTERN = /^[_$A-Z0-9]+$/;

function parseAndBuildMetadata(formatter, code, opts) {
  var ast = parseWithCodeFrame(code, opts.parser);
  var placeholderWhitelist = opts.placeholderWhitelist,
      _opts$placeholderPatt = opts.placeholderPattern,
      placeholderPattern = _opts$placeholderPatt === void 0 ? PATTERN : _opts$placeholderPatt,
      preserveComments = opts.preserveComments;
  t.removePropertiesDeep(ast, {
    preserveComments: preserveComments
  });
  formatter.validate(ast);
  var placeholders = [];
  var placeholderNames = new Set();
  t.traverse(ast, placeholderVisitorHandler, {
    placeholders: placeholders,
    placeholderNames: placeholderNames,
    placeholderWhitelist: placeholderWhitelist,
    placeholderPattern: placeholderPattern
  });
  return {
    ast: ast,
    placeholders: placeholders,
    placeholderNames: placeholderNames
  };
}

function placeholderVisitorHandler(node, ancestors, state) {
  var name;

  if (t.isIdentifier(node)) {
    name = node.name;
  } else if (t.isStringLiteral(node)) {
    name = node.value;
  } else {
    return;
  }

  if ((!state.placeholderPattern || !state.placeholderPattern.test(name)) && (!state.placeholderWhitelist || !state.placeholderWhitelist.has(name))) {
    return;
  }

  ancestors = ancestors.slice();
  var _ancestors = ancestors[ancestors.length - 1],
      parent = _ancestors.node,
      key = _ancestors.key;
  var type;

  if (t.isStringLiteral(node)) {
    type = "string";
  } else if (t.isNewExpression(parent) && key === "arguments" || t.isCallExpression(parent) && key === "arguments" || t.isFunction(parent) && key === "params") {
    type = "param";
  } else if (t.isExpressionStatement(parent)) {
    type = "statement";
    ancestors = ancestors.slice(0, -1);
  } else {
    type = "other";
  }

  state.placeholders.push({
    name: name,
    type: type,
    resolve: function resolve(ast) {
      return resolveAncestors(ast, ancestors);
    },
    isDuplicate: state.placeholderNames.has(name)
  });
  state.placeholderNames.add(name);
}

function resolveAncestors(ast, ancestors) {
  var parent = ast;

  for (var i = 0; i < ancestors.length - 1; i++) {
    var _ancestors$i = ancestors[i],
        _key = _ancestors$i.key,
        _index = _ancestors$i.index;

    if (_index === undefined) {
      parent = parent[_key];
    } else {
      parent = parent[_key][_index];
    }
  }

  var _ancestors2 = ancestors[ancestors.length - 1],
      key = _ancestors2.key,
      index = _ancestors2.index;
  return {
    parent: parent,
    key: key,
    index: index
  };
}

function parseWithCodeFrame(code, parserOpts) {
  parserOpts = Object.assign({
    allowReturnOutsideFunction: true,
    allowSuperOutsideMethod: true,
    sourceType: "module"
  }, parserOpts);

  try {
    return (0, _babylon.parse)(code, parserOpts);
  } catch (err) {
    var loc = err.loc;

    if (loc) {
      err.loc = null;
      err.message += "\n" + (0, _codeFrame.codeFrameColumns)(code, {
        start: loc
      });
    }

    throw err;
  }
}