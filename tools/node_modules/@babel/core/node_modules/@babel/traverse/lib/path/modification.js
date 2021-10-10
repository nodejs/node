"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.insertBefore = insertBefore;
exports._containerInsert = _containerInsert;
exports._containerInsertBefore = _containerInsertBefore;
exports._containerInsertAfter = _containerInsertAfter;
exports.insertAfter = insertAfter;
exports.updateSiblingKeys = updateSiblingKeys;
exports._verifyNodeList = _verifyNodeList;
exports.unshiftContainer = unshiftContainer;
exports.pushContainer = pushContainer;
exports.hoist = hoist;

var _cache = require("../cache");

var _hoister = require("./lib/hoister");

var _index = require("./index");

var _t = require("@babel/types");

const {
  arrowFunctionExpression,
  assertExpression,
  assignmentExpression,
  blockStatement,
  callExpression,
  cloneNode,
  expressionStatement,
  isExpression
} = _t;

function insertBefore(nodes_) {
  this._assertUnremoved();

  const nodes = this._verifyNodeList(nodes_);

  const {
    parentPath
  } = this;

  if (parentPath.isExpressionStatement() || parentPath.isLabeledStatement() || parentPath.isExportNamedDeclaration() || parentPath.isExportDefaultDeclaration() && this.isDeclaration()) {
    return parentPath.insertBefore(nodes);
  } else if (this.isNodeType("Expression") && !this.isJSXElement() || parentPath.isForStatement() && this.key === "init") {
    if (this.node) nodes.push(this.node);
    return this.replaceExpressionWithStatements(nodes);
  } else if (Array.isArray(this.container)) {
    return this._containerInsertBefore(nodes);
  } else if (this.isStatementOrBlock()) {
    const node = this.node;
    const shouldInsertCurrentNode = node && (!this.isExpressionStatement() || node.expression != null);
    this.replaceWith(blockStatement(shouldInsertCurrentNode ? [node] : []));
    return this.unshiftContainer("body", nodes);
  } else {
    throw new Error("We don't know what to do with this node type. " + "We were previously a Statement but we can't fit in here?");
  }
}

function _containerInsert(from, nodes) {
  this.updateSiblingKeys(from, nodes.length);
  const paths = [];
  this.container.splice(from, 0, ...nodes);

  for (let i = 0; i < nodes.length; i++) {
    const to = from + i;
    const path = this.getSibling(to);
    paths.push(path);

    if (this.context && this.context.queue) {
      path.pushContext(this.context);
    }
  }

  const contexts = this._getQueueContexts();

  for (const path of paths) {
    path.setScope();
    path.debug("Inserted.");

    for (const context of contexts) {
      context.maybeQueue(path, true);
    }
  }

  return paths;
}

function _containerInsertBefore(nodes) {
  return this._containerInsert(this.key, nodes);
}

function _containerInsertAfter(nodes) {
  return this._containerInsert(this.key + 1, nodes);
}

function insertAfter(nodes_) {
  this._assertUnremoved();

  const nodes = this._verifyNodeList(nodes_);

  const {
    parentPath
  } = this;

  if (parentPath.isExpressionStatement() || parentPath.isLabeledStatement() || parentPath.isExportNamedDeclaration() || parentPath.isExportDefaultDeclaration() && this.isDeclaration()) {
    return parentPath.insertAfter(nodes.map(node => {
      return isExpression(node) ? expressionStatement(node) : node;
    }));
  } else if (this.isNodeType("Expression") && !this.isJSXElement() && !parentPath.isJSXElement() || parentPath.isForStatement() && this.key === "init") {
    if (this.node) {
      const node = this.node;
      let {
        scope
      } = this;

      if (scope.path.isPattern()) {
        assertExpression(node);
        this.replaceWith(callExpression(arrowFunctionExpression([], node), []));
        this.get("callee.body").insertAfter(nodes);
        return [this];
      }

      if (parentPath.isMethod({
        computed: true,
        key: node
      })) {
        scope = scope.parent;
      }

      const temp = scope.generateDeclaredUidIdentifier();
      nodes.unshift(expressionStatement(assignmentExpression("=", cloneNode(temp), node)));
      nodes.push(expressionStatement(cloneNode(temp)));
    }

    return this.replaceExpressionWithStatements(nodes);
  } else if (Array.isArray(this.container)) {
    return this._containerInsertAfter(nodes);
  } else if (this.isStatementOrBlock()) {
    const node = this.node;
    const shouldInsertCurrentNode = node && (!this.isExpressionStatement() || node.expression != null);
    this.replaceWith(blockStatement(shouldInsertCurrentNode ? [node] : []));
    return this.pushContainer("body", nodes);
  } else {
    throw new Error("We don't know what to do with this node type. " + "We were previously a Statement but we can't fit in here?");
  }
}

function updateSiblingKeys(fromIndex, incrementBy) {
  if (!this.parent) return;

  const paths = _cache.path.get(this.parent);

  for (const [, path] of paths) {
    if (path.key >= fromIndex) {
      path.key += incrementBy;
    }
  }
}

function _verifyNodeList(nodes) {
  if (!nodes) {
    return [];
  }

  if (!Array.isArray(nodes)) {
    nodes = [nodes];
  }

  for (let i = 0; i < nodes.length; i++) {
    const node = nodes[i];
    let msg;

    if (!node) {
      msg = "has falsy node";
    } else if (typeof node !== "object") {
      msg = "contains a non-object node";
    } else if (!node.type) {
      msg = "without a type";
    } else if (node instanceof _index.default) {
      msg = "has a NodePath when it expected a raw object";
    }

    if (msg) {
      const type = Array.isArray(node) ? "array" : typeof node;
      throw new Error(`Node list ${msg} with the index of ${i} and type of ${type}`);
    }
  }

  return nodes;
}

function unshiftContainer(listKey, nodes) {
  this._assertUnremoved();

  nodes = this._verifyNodeList(nodes);

  const path = _index.default.get({
    parentPath: this,
    parent: this.node,
    container: this.node[listKey],
    listKey,
    key: 0
  }).setContext(this.context);

  return path._containerInsertBefore(nodes);
}

function pushContainer(listKey, nodes) {
  this._assertUnremoved();

  const verifiedNodes = this._verifyNodeList(nodes);

  const container = this.node[listKey];

  const path = _index.default.get({
    parentPath: this,
    parent: this.node,
    container: container,
    listKey,
    key: container.length
  }).setContext(this.context);

  return path.replaceWithMultiple(verifiedNodes);
}

function hoist(scope = this.scope) {
  const hoister = new _hoister.default(this, scope);
  return hoister.run();
}