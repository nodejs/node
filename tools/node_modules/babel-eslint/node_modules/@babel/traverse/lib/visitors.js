"use strict";

exports.__esModule = true;
exports.explode = explode;
exports.verify = verify;
exports.merge = merge;

var virtualTypes = _interopRequireWildcard(require("./path/lib/virtual-types"));

var t = _interopRequireWildcard(require("@babel/types"));

var _clone = _interopRequireDefault(require("lodash/clone"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

function explode(visitor) {
  if (visitor._exploded) return visitor;
  visitor._exploded = true;

  for (var nodeType in visitor) {
    if (shouldIgnoreKey(nodeType)) continue;
    var parts = nodeType.split("|");
    if (parts.length === 1) continue;
    var fns = visitor[nodeType];
    delete visitor[nodeType];

    for (var _iterator = parts, _isArray = Array.isArray(_iterator), _i = 0, _iterator = _isArray ? _iterator : _iterator[Symbol.iterator]();;) {
      var _ref;

      if (_isArray) {
        if (_i >= _iterator.length) break;
        _ref = _iterator[_i++];
      } else {
        _i = _iterator.next();
        if (_i.done) break;
        _ref = _i.value;
      }

      var _part = _ref;
      visitor[_part] = fns;
    }
  }

  verify(visitor);
  delete visitor.__esModule;
  ensureEntranceObjects(visitor);
  ensureCallbackArrays(visitor);

  var _arr = Object.keys(visitor);

  for (var _i2 = 0; _i2 < _arr.length; _i2++) {
    var _nodeType = _arr[_i2];
    if (shouldIgnoreKey(_nodeType)) continue;
    var wrapper = virtualTypes[_nodeType];
    if (!wrapper) continue;
    var _fns = visitor[_nodeType];

    for (var type in _fns) {
      _fns[type] = wrapCheck(wrapper, _fns[type]);
    }

    delete visitor[_nodeType];

    if (wrapper.types) {
      var _arr2 = wrapper.types;

      for (var _i4 = 0; _i4 < _arr2.length; _i4++) {
        var _type = _arr2[_i4];

        if (visitor[_type]) {
          mergePair(visitor[_type], _fns);
        } else {
          visitor[_type] = _fns;
        }
      }
    } else {
      mergePair(visitor, _fns);
    }
  }

  for (var _nodeType2 in visitor) {
    if (shouldIgnoreKey(_nodeType2)) continue;
    var _fns2 = visitor[_nodeType2];
    var aliases = t.FLIPPED_ALIAS_KEYS[_nodeType2];
    var deprecratedKey = t.DEPRECATED_KEYS[_nodeType2];

    if (deprecratedKey) {
      console.trace("Visitor defined for " + _nodeType2 + " but it has been renamed to " + deprecratedKey);
      aliases = [deprecratedKey];
    }

    if (!aliases) continue;
    delete visitor[_nodeType2];

    for (var _iterator2 = aliases, _isArray2 = Array.isArray(_iterator2), _i3 = 0, _iterator2 = _isArray2 ? _iterator2 : _iterator2[Symbol.iterator]();;) {
      var _ref2;

      if (_isArray2) {
        if (_i3 >= _iterator2.length) break;
        _ref2 = _iterator2[_i3++];
      } else {
        _i3 = _iterator2.next();
        if (_i3.done) break;
        _ref2 = _i3.value;
      }

      var _alias = _ref2;
      var existing = visitor[_alias];

      if (existing) {
        mergePair(existing, _fns2);
      } else {
        visitor[_alias] = (0, _clone.default)(_fns2);
      }
    }
  }

  for (var _nodeType3 in visitor) {
    if (shouldIgnoreKey(_nodeType3)) continue;
    ensureCallbackArrays(visitor[_nodeType3]);
  }

  return visitor;
}

function verify(visitor) {
  if (visitor._verified) return;

  if (typeof visitor === "function") {
    throw new Error("You passed `traverse()` a function when it expected a visitor object, " + "are you sure you didn't mean `{ enter: Function }`?");
  }

  for (var nodeType in visitor) {
    if (nodeType === "enter" || nodeType === "exit") {
      validateVisitorMethods(nodeType, visitor[nodeType]);
    }

    if (shouldIgnoreKey(nodeType)) continue;

    if (t.TYPES.indexOf(nodeType) < 0) {
      throw new Error("You gave us a visitor for the node type " + nodeType + " but it's not a valid type");
    }

    var visitors = visitor[nodeType];

    if (typeof visitors === "object") {
      for (var visitorKey in visitors) {
        if (visitorKey === "enter" || visitorKey === "exit") {
          validateVisitorMethods(nodeType + "." + visitorKey, visitors[visitorKey]);
        } else {
          throw new Error("You passed `traverse()` a visitor object with the property " + (nodeType + " that has the invalid property " + visitorKey));
        }
      }
    }
  }

  visitor._verified = true;
}

function validateVisitorMethods(path, val) {
  var fns = [].concat(val);

  for (var _iterator3 = fns, _isArray3 = Array.isArray(_iterator3), _i5 = 0, _iterator3 = _isArray3 ? _iterator3 : _iterator3[Symbol.iterator]();;) {
    var _ref3;

    if (_isArray3) {
      if (_i5 >= _iterator3.length) break;
      _ref3 = _iterator3[_i5++];
    } else {
      _i5 = _iterator3.next();
      if (_i5.done) break;
      _ref3 = _i5.value;
    }

    var _fn = _ref3;

    if (typeof _fn !== "function") {
      throw new TypeError("Non-function found defined in " + path + " with type " + typeof _fn);
    }
  }
}

function merge(visitors, states, wrapper) {
  if (states === void 0) {
    states = [];
  }

  var rootVisitor = {};

  for (var i = 0; i < visitors.length; i++) {
    var visitor = visitors[i];
    var state = states[i];
    explode(visitor);

    for (var type in visitor) {
      var visitorType = visitor[type];

      if (state || wrapper) {
        visitorType = wrapWithStateOrWrapper(visitorType, state, wrapper);
      }

      var nodeVisitor = rootVisitor[type] = rootVisitor[type] || {};
      mergePair(nodeVisitor, visitorType);
    }
  }

  return rootVisitor;
}

function wrapWithStateOrWrapper(oldVisitor, state, wrapper) {
  var newVisitor = {};

  var _loop = function _loop(key) {
    var fns = oldVisitor[key];
    if (!Array.isArray(fns)) return "continue";
    fns = fns.map(function (fn) {
      var newFn = fn;

      if (state) {
        newFn = function newFn(path) {
          return fn.call(state, path, state);
        };
      }

      if (wrapper) {
        newFn = wrapper(state.key, key, newFn);
      }

      return newFn;
    });
    newVisitor[key] = fns;
  };

  for (var key in oldVisitor) {
    var _ret = _loop(key);

    if (_ret === "continue") continue;
  }

  return newVisitor;
}

function ensureEntranceObjects(obj) {
  for (var key in obj) {
    if (shouldIgnoreKey(key)) continue;
    var fns = obj[key];

    if (typeof fns === "function") {
      obj[key] = {
        enter: fns
      };
    }
  }
}

function ensureCallbackArrays(obj) {
  if (obj.enter && !Array.isArray(obj.enter)) obj.enter = [obj.enter];
  if (obj.exit && !Array.isArray(obj.exit)) obj.exit = [obj.exit];
}

function wrapCheck(wrapper, fn) {
  var newFn = function newFn(path) {
    if (wrapper.checkPath(path)) {
      return fn.apply(this, arguments);
    }
  };

  newFn.toString = function () {
    return fn.toString();
  };

  return newFn;
}

function shouldIgnoreKey(key) {
  if (key[0] === "_") return true;
  if (key === "enter" || key === "exit" || key === "shouldSkip") return true;

  if (key === "blacklist" || key === "noScope" || key === "skipKeys") {
    return true;
  }

  return false;
}

function mergePair(dest, src) {
  for (var key in src) {
    dest[key] = [].concat(dest[key] || [], src[key]);
  }
}