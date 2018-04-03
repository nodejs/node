"use strict";

exports.__esModule = true;
exports.getOpposite = getOpposite;
exports.getCompletionRecords = getCompletionRecords;
exports.getSibling = getSibling;
exports.getPrevSibling = getPrevSibling;
exports.getNextSibling = getNextSibling;
exports.getAllNextSiblings = getAllNextSiblings;
exports.getAllPrevSiblings = getAllPrevSiblings;
exports.get = get;
exports._getKey = _getKey;
exports._getPattern = _getPattern;
exports.getBindingIdentifiers = getBindingIdentifiers;
exports.getOuterBindingIdentifiers = getOuterBindingIdentifiers;
exports.getBindingIdentifierPaths = getBindingIdentifierPaths;
exports.getOuterBindingIdentifierPaths = getOuterBindingIdentifierPaths;

var _index = _interopRequireDefault(require("./index"));

var t = _interopRequireWildcard(require("@babel/types"));

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function getOpposite() {
  if (this.key === "left") {
    return this.getSibling("right");
  } else if (this.key === "right") {
    return this.getSibling("left");
  }
}

function addCompletionRecords(path, paths) {
  if (path) return paths.concat(path.getCompletionRecords());
  return paths;
}

function getCompletionRecords() {
  var paths = [];

  if (this.isIfStatement()) {
    paths = addCompletionRecords(this.get("consequent"), paths);
    paths = addCompletionRecords(this.get("alternate"), paths);
  } else if (this.isDoExpression() || this.isFor() || this.isWhile()) {
    paths = addCompletionRecords(this.get("body"), paths);
  } else if (this.isProgram() || this.isBlockStatement()) {
    paths = addCompletionRecords(this.get("body").pop(), paths);
  } else if (this.isFunction()) {
    return this.get("body").getCompletionRecords();
  } else if (this.isTryStatement()) {
    paths = addCompletionRecords(this.get("block"), paths);
    paths = addCompletionRecords(this.get("handler"), paths);
    paths = addCompletionRecords(this.get("finalizer"), paths);
  } else if (this.isCatchClause()) {
    paths = addCompletionRecords(this.get("body"), paths);
  } else {
    paths.push(this);
  }

  return paths;
}

function getSibling(key) {
  return _index.default.get({
    parentPath: this.parentPath,
    parent: this.parent,
    container: this.container,
    listKey: this.listKey,
    key: key
  });
}

function getPrevSibling() {
  return this.getSibling(this.key - 1);
}

function getNextSibling() {
  return this.getSibling(this.key + 1);
}

function getAllNextSiblings() {
  var _key = this.key;
  var sibling = this.getSibling(++_key);
  var siblings = [];

  while (sibling.node) {
    siblings.push(sibling);
    sibling = this.getSibling(++_key);
  }

  return siblings;
}

function getAllPrevSiblings() {
  var _key = this.key;
  var sibling = this.getSibling(--_key);
  var siblings = [];

  while (sibling.node) {
    siblings.push(sibling);
    sibling = this.getSibling(--_key);
  }

  return siblings;
}

function get(key, context) {
  if (context === true) context = this.context;
  var parts = key.split(".");

  if (parts.length === 1) {
    return this._getKey(key, context);
  } else {
    return this._getPattern(parts, context);
  }
}

function _getKey(key, context) {
  var _this = this;

  var node = this.node;
  var container = node[key];

  if (Array.isArray(container)) {
    return container.map(function (_, i) {
      return _index.default.get({
        listKey: key,
        parentPath: _this,
        parent: node,
        container: container,
        key: i
      }).setContext(context);
    });
  } else {
    return _index.default.get({
      parentPath: this,
      parent: node,
      container: node,
      key: key
    }).setContext(context);
  }
}

function _getPattern(parts, context) {
  var path = this;
  var _arr = parts;

  for (var _i = 0; _i < _arr.length; _i++) {
    var part = _arr[_i];

    if (part === ".") {
      path = path.parentPath;
    } else {
      if (Array.isArray(path)) {
        path = path[part];
      } else {
        path = path.get(part, context);
      }
    }
  }

  return path;
}

function getBindingIdentifiers(duplicates) {
  return t.getBindingIdentifiers(this.node, duplicates);
}

function getOuterBindingIdentifiers(duplicates) {
  return t.getOuterBindingIdentifiers(this.node, duplicates);
}

function getBindingIdentifierPaths(duplicates, outerOnly) {
  if (duplicates === void 0) {
    duplicates = false;
  }

  if (outerOnly === void 0) {
    outerOnly = false;
  }

  var path = this;
  var search = [].concat(path);
  var ids = Object.create(null);

  while (search.length) {
    var id = search.shift();
    if (!id) continue;
    if (!id.node) continue;
    var keys = t.getBindingIdentifiers.keys[id.node.type];

    if (id.isIdentifier()) {
      if (duplicates) {
        var _ids = ids[id.node.name] = ids[id.node.name] || [];

        _ids.push(id);
      } else {
        ids[id.node.name] = id;
      }

      continue;
    }

    if (id.isExportDeclaration()) {
      var declaration = id.get("declaration");

      if (declaration.isDeclaration()) {
        search.push(declaration);
      }

      continue;
    }

    if (outerOnly) {
      if (id.isFunctionDeclaration()) {
        search.push(id.get("id"));
        continue;
      }

      if (id.isFunctionExpression()) {
        continue;
      }
    }

    if (keys) {
      for (var i = 0; i < keys.length; i++) {
        var key = keys[i];
        var child = id.get(key);

        if (Array.isArray(child) || child.node) {
          search = search.concat(child);
        }
      }
    }
  }

  return ids;
}

function getOuterBindingIdentifierPaths(duplicates) {
  return this.getBindingIdentifierPaths(duplicates, true);
}