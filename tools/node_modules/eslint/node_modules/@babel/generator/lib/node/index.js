"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.needsParens = needsParens;
exports.needsWhitespace = needsWhitespace;
exports.needsWhitespaceAfter = needsWhitespaceAfter;
exports.needsWhitespaceBefore = needsWhitespaceBefore;

var whitespace = require("./whitespace");

var parens = require("./parentheses");

var _t = require("@babel/types");

const {
  FLIPPED_ALIAS_KEYS,
  isCallExpression,
  isExpressionStatement,
  isMemberExpression,
  isNewExpression
} = _t;

function expandAliases(obj) {
  const newObj = {};

  function add(type, func) {
    const fn = newObj[type];
    newObj[type] = fn ? function (node, parent, stack) {
      const result = fn(node, parent, stack);
      return result == null ? func(node, parent, stack) : result;
    } : func;
  }

  for (const type of Object.keys(obj)) {
    const aliases = FLIPPED_ALIAS_KEYS[type];

    if (aliases) {
      for (const alias of aliases) {
        add(alias, obj[type]);
      }
    } else {
      add(type, obj[type]);
    }
  }

  return newObj;
}

const expandedParens = expandAliases(parens);
const expandedWhitespaceNodes = expandAliases(whitespace.nodes);
const expandedWhitespaceList = expandAliases(whitespace.list);

function find(obj, node, parent, printStack) {
  const fn = obj[node.type];
  return fn ? fn(node, parent, printStack) : null;
}

function isOrHasCallExpression(node) {
  if (isCallExpression(node)) {
    return true;
  }

  return isMemberExpression(node) && isOrHasCallExpression(node.object);
}

function needsWhitespace(node, parent, type) {
  if (!node) return false;

  if (isExpressionStatement(node)) {
    node = node.expression;
  }

  let linesInfo = find(expandedWhitespaceNodes, node, parent);

  if (!linesInfo) {
    const items = find(expandedWhitespaceList, node, parent);

    if (items) {
      for (let i = 0; i < items.length; i++) {
        linesInfo = needsWhitespace(items[i], node, type);
        if (linesInfo) break;
      }
    }
  }

  if (typeof linesInfo === "object" && linesInfo !== null) {
    return linesInfo[type] || false;
  }

  return false;
}

function needsWhitespaceBefore(node, parent) {
  return needsWhitespace(node, parent, "before");
}

function needsWhitespaceAfter(node, parent) {
  return needsWhitespace(node, parent, "after");
}

function needsParens(node, parent, printStack) {
  if (!parent) return false;

  if (isNewExpression(parent) && parent.callee === node) {
    if (isOrHasCallExpression(node)) return true;
  }

  return find(expandedParens, node, parent, printStack);
}