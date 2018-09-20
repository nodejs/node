"use strict";

exports.__esModule = true;
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

var _hoister = _interopRequireDefault(require("./lib/hoister"));

var _index = _interopRequireDefault(require("./index"));

var t = _interopRequireWildcard(require("@babel/types"));

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function insertBefore(nodes) {
  this._assertUnremoved();

  nodes = this._verifyNodeList(nodes);

  if (this.parentPath.isExpressionStatement() || this.parentPath.isLabeledStatement() || this.parentPath.isExportDeclaration()) {
    return this.parentPath.insertBefore(nodes);
  } else if (this.isNodeType("Expression") && this.listKey !== "params" && this.listKey !== "arguments" || this.parentPath.isForStatement() && this.key === "init") {
    if (this.node) nodes.push(this.node);
    return this.replaceExpressionWithStatements(nodes);
  } else if (Array.isArray(this.container)) {
    return this._containerInsertBefore(nodes);
  } else if (this.isStatementOrBlock()) {
    var shouldInsertCurrentNode = this.node && (!this.isExpressionStatement() || this.node.expression != null);
    this.replaceWith(t.blockStatement(shouldInsertCurrentNode ? [this.node] : []));
    return this.unshiftContainer("body", nodes);
  } else {
    throw new Error("We don't know what to do with this node type. " + "We were previously a Statement but we can't fit in here?");
  }
}

function _containerInsert(from, nodes) {
  var _container;

  this.updateSiblingKeys(from, nodes.length);
  var paths = [];

  (_container = this.container).splice.apply(_container, [from, 0].concat(nodes));

  for (var i = 0; i < nodes.length; i++) {
    var to = from + i;
    var path = this.getSibling("" + to);
    paths.push(path);

    if (this.context && this.context.queue) {
      path.pushContext(this.context);
    }
  }

  var contexts = this._getQueueContexts();

  for (var _i = 0; _i < paths.length; _i++) {
    var _path = paths[_i];

    _path.setScope();

    _path.debug("Inserted.");

    for (var _iterator = contexts, _isArray = Array.isArray(_iterator), _i2 = 0, _iterator = _isArray ? _iterator : _iterator[Symbol.iterator]();;) {
      var _ref;

      if (_isArray) {
        if (_i2 >= _iterator.length) break;
        _ref = _iterator[_i2++];
      } else {
        _i2 = _iterator.next();
        if (_i2.done) break;
        _ref = _i2.value;
      }

      var _context = _ref;

      _context.maybeQueue(_path, true);
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

function insertAfter(nodes) {
  this._assertUnremoved();

  nodes = this._verifyNodeList(nodes);

  if (this.parentPath.isExpressionStatement() || this.parentPath.isLabeledStatement() || this.parentPath.isExportDeclaration()) {
    return this.parentPath.insertAfter(nodes);
  } else if (this.isNodeType("Expression") || this.parentPath.isForStatement() && this.key === "init") {
    if (this.node) {
      var temp = this.scope.generateDeclaredUidIdentifier();
      nodes.unshift(t.expressionStatement(t.assignmentExpression("=", temp, this.node)));
      nodes.push(t.expressionStatement(temp));
    }

    return this.replaceExpressionWithStatements(nodes);
  } else if (Array.isArray(this.container)) {
    return this._containerInsertAfter(nodes);
  } else if (this.isStatementOrBlock()) {
    var shouldInsertCurrentNode = this.node && (!this.isExpressionStatement() || this.node.expression != null);
    this.replaceWith(t.blockStatement(shouldInsertCurrentNode ? [this.node] : []));
    return this.pushContainer("body", nodes);
  } else {
    throw new Error("We don't know what to do with this node type. " + "We were previously a Statement but we can't fit in here?");
  }
}

function updateSiblingKeys(fromIndex, incrementBy) {
  if (!this.parent) return;

  var paths = _cache.path.get(this.parent);

  for (var i = 0; i < paths.length; i++) {
    var path = paths[i];

    if (path.key >= fromIndex) {
      path.key += incrementBy;
    }
  }
}

function _verifyNodeList(nodes) {
  if (!nodes) {
    return [];
  }

  if (nodes.constructor !== Array) {
    nodes = [nodes];
  }

  for (var i = 0; i < nodes.length; i++) {
    var node = nodes[i];
    var msg = void 0;

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
      var type = Array.isArray(node) ? "array" : typeof node;
      throw new Error("Node list " + msg + " with the index of " + i + " and type of " + type);
    }
  }

  return nodes;
}

function unshiftContainer(listKey, nodes) {
  this._assertUnremoved();

  nodes = this._verifyNodeList(nodes);

  var path = _index.default.get({
    parentPath: this,
    parent: this.node,
    container: this.node[listKey],
    listKey: listKey,
    key: 0
  });

  return path.insertBefore(nodes);
}

function pushContainer(listKey, nodes) {
  this._assertUnremoved();

  nodes = this._verifyNodeList(nodes);
  var container = this.node[listKey];

  var path = _index.default.get({
    parentPath: this,
    parent: this.node,
    container: container,
    listKey: listKey,
    key: container.length
  });

  return path.replaceWithMultiple(nodes);
}

function hoist(scope) {
  if (scope === void 0) {
    scope = this.scope;
  }

  var hoister = new _hoister.default(this, scope);
  return hoister.run();
}