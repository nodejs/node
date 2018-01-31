"use strict";

exports.__esModule = true;
exports.findParent = findParent;
exports.find = find;
exports.getFunctionParent = getFunctionParent;
exports.getStatementParent = getStatementParent;
exports.getEarliestCommonAncestorFrom = getEarliestCommonAncestorFrom;
exports.getDeepestCommonAncestorFrom = getDeepestCommonAncestorFrom;
exports.getAncestry = getAncestry;
exports.isAncestor = isAncestor;
exports.isDescendant = isDescendant;
exports.inType = inType;

var t = _interopRequireWildcard(require("@babel/types"));

var _index = _interopRequireDefault(require("./index"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

function findParent(callback) {
  var path = this;

  while (path = path.parentPath) {
    if (callback(path)) return path;
  }

  return null;
}

function find(callback) {
  var path = this;

  do {
    if (callback(path)) return path;
  } while (path = path.parentPath);

  return null;
}

function getFunctionParent() {
  return this.findParent(function (p) {
    return p.isFunction();
  });
}

function getStatementParent() {
  var path = this;

  do {
    if (!path.parentPath || Array.isArray(path.container) && path.isStatement()) {
      break;
    } else {
      path = path.parentPath;
    }
  } while (path);

  if (path && (path.isProgram() || path.isFile())) {
    throw new Error("File/Program node, we can't possibly find a statement parent to this");
  }

  return path;
}

function getEarliestCommonAncestorFrom(paths) {
  return this.getDeepestCommonAncestorFrom(paths, function (deepest, i, ancestries) {
    var earliest;
    var keys = t.VISITOR_KEYS[deepest.type];
    var _arr = ancestries;

    for (var _i = 0; _i < _arr.length; _i++) {
      var ancestry = _arr[_i];
      var path = ancestry[i + 1];

      if (!earliest) {
        earliest = path;
        continue;
      }

      if (path.listKey && earliest.listKey === path.listKey) {
        if (path.key < earliest.key) {
          earliest = path;
          continue;
        }
      }

      var earliestKeyIndex = keys.indexOf(earliest.parentKey);
      var currentKeyIndex = keys.indexOf(path.parentKey);

      if (earliestKeyIndex > currentKeyIndex) {
        earliest = path;
      }
    }

    return earliest;
  });
}

function getDeepestCommonAncestorFrom(paths, filter) {
  var _this = this;

  if (!paths.length) {
    return this;
  }

  if (paths.length === 1) {
    return paths[0];
  }

  var minDepth = Infinity;
  var lastCommonIndex, lastCommon;
  var ancestries = paths.map(function (path) {
    var ancestry = [];

    do {
      ancestry.unshift(path);
    } while ((path = path.parentPath) && path !== _this);

    if (ancestry.length < minDepth) {
      minDepth = ancestry.length;
    }

    return ancestry;
  });
  var first = ancestries[0];

  depthLoop: for (var i = 0; i < minDepth; i++) {
    var shouldMatch = first[i];
    var _arr2 = ancestries;

    for (var _i2 = 0; _i2 < _arr2.length; _i2++) {
      var ancestry = _arr2[_i2];

      if (ancestry[i] !== shouldMatch) {
        break depthLoop;
      }
    }

    lastCommonIndex = i;
    lastCommon = shouldMatch;
  }

  if (lastCommon) {
    if (filter) {
      return filter(lastCommon, lastCommonIndex, ancestries);
    } else {
      return lastCommon;
    }
  } else {
    throw new Error("Couldn't find intersection");
  }
}

function getAncestry() {
  var path = this;
  var paths = [];

  do {
    paths.push(path);
  } while (path = path.parentPath);

  return paths;
}

function isAncestor(maybeDescendant) {
  return maybeDescendant.isDescendant(this);
}

function isDescendant(maybeAncestor) {
  return !!this.findParent(function (parent) {
    return parent === maybeAncestor;
  });
}

function inType() {
  var path = this;

  while (path) {
    var _arr3 = arguments;

    for (var _i3 = 0; _i3 < _arr3.length; _i3++) {
      var type = _arr3[_i3];
      if (path.node.type === type) return true;
    }

    path = path.parentPath;
  }

  return false;
}