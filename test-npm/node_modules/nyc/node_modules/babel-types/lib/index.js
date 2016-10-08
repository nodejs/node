"use strict";

exports.__esModule = true;
exports.createTypeAnnotationBasedOnTypeof = exports.removeTypeDuplicates = exports.createUnionTypeAnnotation = exports.valueToNode = exports.toBlock = exports.toExpression = exports.toStatement = exports.toBindingIdentifierName = exports.toIdentifier = exports.toKeyAlias = exports.toSequenceExpression = exports.toComputedKey = exports.isImmutable = exports.isScope = exports.isSpecifierDefault = exports.isVar = exports.isBlockScoped = exports.isLet = exports.isValidIdentifier = exports.isReferenced = exports.isBinding = exports.getOuterBindingIdentifiers = exports.getBindingIdentifiers = exports.TYPES = exports.react = exports.DEPRECATED_KEYS = exports.BUILDER_KEYS = exports.NODE_FIELDS = exports.ALIAS_KEYS = exports.VISITOR_KEYS = exports.NOT_LOCAL_BINDING = exports.BLOCK_SCOPED_SYMBOL = exports.INHERIT_KEYS = exports.UNARY_OPERATORS = exports.STRING_UNARY_OPERATORS = exports.NUMBER_UNARY_OPERATORS = exports.BOOLEAN_UNARY_OPERATORS = exports.BINARY_OPERATORS = exports.NUMBER_BINARY_OPERATORS = exports.BOOLEAN_BINARY_OPERATORS = exports.COMPARISON_BINARY_OPERATORS = exports.EQUALITY_BINARY_OPERATORS = exports.BOOLEAN_NUMBER_BINARY_OPERATORS = exports.UPDATE_OPERATORS = exports.LOGICAL_OPERATORS = exports.COMMENT_KEYS = exports.FOR_INIT_KEYS = exports.FLATTENABLE_KEYS = exports.STATEMENT_OR_BLOCK_KEYS = undefined;

var _getIterator2 = require("babel-runtime/core-js/get-iterator");

var _getIterator3 = _interopRequireDefault(_getIterator2);

var _keys = require("babel-runtime/core-js/object/keys");

var _keys2 = _interopRequireDefault(_keys);

var _stringify = require("babel-runtime/core-js/json/stringify");

var _stringify2 = _interopRequireDefault(_stringify);

var _constants = require("./constants");

Object.defineProperty(exports, "STATEMENT_OR_BLOCK_KEYS", {
  enumerable: true,
  get: function get() {
    return _constants.STATEMENT_OR_BLOCK_KEYS;
  }
});
Object.defineProperty(exports, "FLATTENABLE_KEYS", {
  enumerable: true,
  get: function get() {
    return _constants.FLATTENABLE_KEYS;
  }
});
Object.defineProperty(exports, "FOR_INIT_KEYS", {
  enumerable: true,
  get: function get() {
    return _constants.FOR_INIT_KEYS;
  }
});
Object.defineProperty(exports, "COMMENT_KEYS", {
  enumerable: true,
  get: function get() {
    return _constants.COMMENT_KEYS;
  }
});
Object.defineProperty(exports, "LOGICAL_OPERATORS", {
  enumerable: true,
  get: function get() {
    return _constants.LOGICAL_OPERATORS;
  }
});
Object.defineProperty(exports, "UPDATE_OPERATORS", {
  enumerable: true,
  get: function get() {
    return _constants.UPDATE_OPERATORS;
  }
});
Object.defineProperty(exports, "BOOLEAN_NUMBER_BINARY_OPERATORS", {
  enumerable: true,
  get: function get() {
    return _constants.BOOLEAN_NUMBER_BINARY_OPERATORS;
  }
});
Object.defineProperty(exports, "EQUALITY_BINARY_OPERATORS", {
  enumerable: true,
  get: function get() {
    return _constants.EQUALITY_BINARY_OPERATORS;
  }
});
Object.defineProperty(exports, "COMPARISON_BINARY_OPERATORS", {
  enumerable: true,
  get: function get() {
    return _constants.COMPARISON_BINARY_OPERATORS;
  }
});
Object.defineProperty(exports, "BOOLEAN_BINARY_OPERATORS", {
  enumerable: true,
  get: function get() {
    return _constants.BOOLEAN_BINARY_OPERATORS;
  }
});
Object.defineProperty(exports, "NUMBER_BINARY_OPERATORS", {
  enumerable: true,
  get: function get() {
    return _constants.NUMBER_BINARY_OPERATORS;
  }
});
Object.defineProperty(exports, "BINARY_OPERATORS", {
  enumerable: true,
  get: function get() {
    return _constants.BINARY_OPERATORS;
  }
});
Object.defineProperty(exports, "BOOLEAN_UNARY_OPERATORS", {
  enumerable: true,
  get: function get() {
    return _constants.BOOLEAN_UNARY_OPERATORS;
  }
});
Object.defineProperty(exports, "NUMBER_UNARY_OPERATORS", {
  enumerable: true,
  get: function get() {
    return _constants.NUMBER_UNARY_OPERATORS;
  }
});
Object.defineProperty(exports, "STRING_UNARY_OPERATORS", {
  enumerable: true,
  get: function get() {
    return _constants.STRING_UNARY_OPERATORS;
  }
});
Object.defineProperty(exports, "UNARY_OPERATORS", {
  enumerable: true,
  get: function get() {
    return _constants.UNARY_OPERATORS;
  }
});
Object.defineProperty(exports, "INHERIT_KEYS", {
  enumerable: true,
  get: function get() {
    return _constants.INHERIT_KEYS;
  }
});
Object.defineProperty(exports, "BLOCK_SCOPED_SYMBOL", {
  enumerable: true,
  get: function get() {
    return _constants.BLOCK_SCOPED_SYMBOL;
  }
});
Object.defineProperty(exports, "NOT_LOCAL_BINDING", {
  enumerable: true,
  get: function get() {
    return _constants.NOT_LOCAL_BINDING;
  }
});
exports.is = is;
exports.isType = isType;
exports.validate = validate;
exports.shallowEqual = shallowEqual;
exports.appendToMemberExpression = appendToMemberExpression;
exports.prependToMemberExpression = prependToMemberExpression;
exports.ensureBlock = ensureBlock;
exports.clone = clone;
exports.cloneWithoutLoc = cloneWithoutLoc;
exports.cloneDeep = cloneDeep;
exports.buildMatchMemberExpression = buildMatchMemberExpression;
exports.removeComments = removeComments;
exports.inheritsComments = inheritsComments;
exports.inheritTrailingComments = inheritTrailingComments;
exports.inheritLeadingComments = inheritLeadingComments;
exports.inheritInnerComments = inheritInnerComments;
exports.inherits = inherits;
exports.assertNode = assertNode;
exports.isNode = isNode;

var _retrievers = require("./retrievers");

Object.defineProperty(exports, "getBindingIdentifiers", {
  enumerable: true,
  get: function get() {
    return _retrievers.getBindingIdentifiers;
  }
});
Object.defineProperty(exports, "getOuterBindingIdentifiers", {
  enumerable: true,
  get: function get() {
    return _retrievers.getOuterBindingIdentifiers;
  }
});

var _validators = require("./validators");

Object.defineProperty(exports, "isBinding", {
  enumerable: true,
  get: function get() {
    return _validators.isBinding;
  }
});
Object.defineProperty(exports, "isReferenced", {
  enumerable: true,
  get: function get() {
    return _validators.isReferenced;
  }
});
Object.defineProperty(exports, "isValidIdentifier", {
  enumerable: true,
  get: function get() {
    return _validators.isValidIdentifier;
  }
});
Object.defineProperty(exports, "isLet", {
  enumerable: true,
  get: function get() {
    return _validators.isLet;
  }
});
Object.defineProperty(exports, "isBlockScoped", {
  enumerable: true,
  get: function get() {
    return _validators.isBlockScoped;
  }
});
Object.defineProperty(exports, "isVar", {
  enumerable: true,
  get: function get() {
    return _validators.isVar;
  }
});
Object.defineProperty(exports, "isSpecifierDefault", {
  enumerable: true,
  get: function get() {
    return _validators.isSpecifierDefault;
  }
});
Object.defineProperty(exports, "isScope", {
  enumerable: true,
  get: function get() {
    return _validators.isScope;
  }
});
Object.defineProperty(exports, "isImmutable", {
  enumerable: true,
  get: function get() {
    return _validators.isImmutable;
  }
});

var _converters = require("./converters");

Object.defineProperty(exports, "toComputedKey", {
  enumerable: true,
  get: function get() {
    return _converters.toComputedKey;
  }
});
Object.defineProperty(exports, "toSequenceExpression", {
  enumerable: true,
  get: function get() {
    return _converters.toSequenceExpression;
  }
});
Object.defineProperty(exports, "toKeyAlias", {
  enumerable: true,
  get: function get() {
    return _converters.toKeyAlias;
  }
});
Object.defineProperty(exports, "toIdentifier", {
  enumerable: true,
  get: function get() {
    return _converters.toIdentifier;
  }
});
Object.defineProperty(exports, "toBindingIdentifierName", {
  enumerable: true,
  get: function get() {
    return _converters.toBindingIdentifierName;
  }
});
Object.defineProperty(exports, "toStatement", {
  enumerable: true,
  get: function get() {
    return _converters.toStatement;
  }
});
Object.defineProperty(exports, "toExpression", {
  enumerable: true,
  get: function get() {
    return _converters.toExpression;
  }
});
Object.defineProperty(exports, "toBlock", {
  enumerable: true,
  get: function get() {
    return _converters.toBlock;
  }
});
Object.defineProperty(exports, "valueToNode", {
  enumerable: true,
  get: function get() {
    return _converters.valueToNode;
  }
});

var _flow = require("./flow");

Object.defineProperty(exports, "createUnionTypeAnnotation", {
  enumerable: true,
  get: function get() {
    return _flow.createUnionTypeAnnotation;
  }
});
Object.defineProperty(exports, "removeTypeDuplicates", {
  enumerable: true,
  get: function get() {
    return _flow.removeTypeDuplicates;
  }
});
Object.defineProperty(exports, "createTypeAnnotationBasedOnTypeof", {
  enumerable: true,
  get: function get() {
    return _flow.createTypeAnnotationBasedOnTypeof;
  }
});

var _toFastProperties = require("to-fast-properties");

var _toFastProperties2 = _interopRequireDefault(_toFastProperties);

var _compact = require("lodash/compact");

var _compact2 = _interopRequireDefault(_compact);

var _clone = require("lodash/clone");

var _clone2 = _interopRequireDefault(_clone);

var _each = require("lodash/each");

var _each2 = _interopRequireDefault(_each);

var _uniq = require("lodash/uniq");

var _uniq2 = _interopRequireDefault(_uniq);

require("./definitions/init");

var _definitions = require("./definitions");

var _react2 = require("./react");

var _react = _interopRequireWildcard(_react2);

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) newObj[key] = obj[key]; } } newObj.default = obj; return newObj; } }

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

var t = exports;

/**
 * Registers `is[Type]` and `assert[Type]` generated functions for a given `type`.
 * Pass `skipAliasCheck` to force it to directly compare `node.type` with `type`.
 */

function registerType(type) {
  var is = t["is" + type];
  if (!is) {
    is = t["is" + type] = function (node, opts) {
      return t.is(type, node, opts);
    };
  }

  t["assert" + type] = function (node, opts) {
    opts = opts || {};
    if (!is(node, opts)) {
      throw new Error("Expected type " + (0, _stringify2.default)(type) + " with option " + (0, _stringify2.default)(opts));
    }
  };
}

//

exports.VISITOR_KEYS = _definitions.VISITOR_KEYS;
exports.ALIAS_KEYS = _definitions.ALIAS_KEYS;
exports.NODE_FIELDS = _definitions.NODE_FIELDS;
exports.BUILDER_KEYS = _definitions.BUILDER_KEYS;
exports.DEPRECATED_KEYS = _definitions.DEPRECATED_KEYS;
exports.react = _react;

/**
 * Registers `is[Type]` and `assert[Type]` for all types.
 */

for (var type in t.VISITOR_KEYS) {
  registerType(type);
}

/**
 * Flip `ALIAS_KEYS` for faster access in the reverse direction.
 */

t.FLIPPED_ALIAS_KEYS = {};

(0, _each2.default)(t.ALIAS_KEYS, function (aliases, type) {
  (0, _each2.default)(aliases, function (alias) {
    var types = t.FLIPPED_ALIAS_KEYS[alias] = t.FLIPPED_ALIAS_KEYS[alias] || [];
    types.push(type);
  });
});

/**
 * Registers `is[Alias]` and `assert[Alias]` functions for all aliases.
 */

(0, _each2.default)(t.FLIPPED_ALIAS_KEYS, function (types, type) {
  t[type.toUpperCase() + "_TYPES"] = types;
  registerType(type);
});

var TYPES = exports.TYPES = (0, _keys2.default)(t.VISITOR_KEYS).concat((0, _keys2.default)(t.FLIPPED_ALIAS_KEYS)).concat((0, _keys2.default)(t.DEPRECATED_KEYS));

/**
 * Returns whether `node` is of given `type`.
 *
 * For better performance, use this instead of `is[Type]` when `type` is unknown.
 * Optionally, pass `skipAliasCheck` to directly compare `node.type` with `type`.
 */

function is(type, node, opts) {
  if (!node) return false;

  var matches = isType(node.type, type);
  if (!matches) return false;

  if (typeof opts === "undefined") {
    return true;
  } else {
    return t.shallowEqual(node, opts);
  }
}

/**
 * Test if a `nodeType` is a `targetType` or if `targetType` is an alias of `nodeType`.
 */

function isType(nodeType, targetType) {
  if (nodeType === targetType) return true;

  // This is a fast-path. If the test above failed, but an alias key is found, then the
  // targetType was a primary node type, so there's no need to check the aliases.
  if (t.ALIAS_KEYS[targetType]) return false;

  var aliases = t.FLIPPED_ALIAS_KEYS[targetType];
  if (aliases) {
    if (aliases[0] === nodeType) return true;

    for (var _iterator = aliases, _isArray = Array.isArray(_iterator), _i = 0, _iterator = _isArray ? _iterator : (0, _getIterator3.default)(_iterator);;) {
      var _ref;

      if (_isArray) {
        if (_i >= _iterator.length) break;
        _ref = _iterator[_i++];
      } else {
        _i = _iterator.next();
        if (_i.done) break;
        _ref = _i.value;
      }

      var alias = _ref;

      if (nodeType === alias) return true;
    }
  }

  return false;
}

/**
 * Description
 */

(0, _each2.default)(t.BUILDER_KEYS, function (keys, type) {
  function builder() {
    if (arguments.length > keys.length) {
      throw new Error("t." + type + ": Too many arguments passed. Received " + arguments.length + " but can receive " + ("no more than " + keys.length));
    }

    var node = {};
    node.type = type;

    var i = 0;

    for (var _iterator2 = keys, _isArray2 = Array.isArray(_iterator2), _i2 = 0, _iterator2 = _isArray2 ? _iterator2 : (0, _getIterator3.default)(_iterator2);;) {
      var _ref2;

      if (_isArray2) {
        if (_i2 >= _iterator2.length) break;
        _ref2 = _iterator2[_i2++];
      } else {
        _i2 = _iterator2.next();
        if (_i2.done) break;
        _ref2 = _i2.value;
      }

      var _key = _ref2;

      var field = t.NODE_FIELDS[type][_key];

      var arg = arguments[i++];
      if (arg === undefined) arg = (0, _clone2.default)(field.default);

      node[_key] = arg;
    }

    for (var key in node) {
      validate(node, key, node[key]);
    }

    return node;
  }

  t[type] = builder;
  t[type[0].toLowerCase() + type.slice(1)] = builder;
});

/**
 * Description
 */

var _loop = function _loop(_type) {
  var newType = t.DEPRECATED_KEYS[_type];

  function proxy(fn) {
    return function () {
      console.trace("The node type " + _type + " has been renamed to " + newType);
      return fn.apply(this, arguments);
    };
  }

  t[_type] = t[_type[0].toLowerCase() + _type.slice(1)] = proxy(t[newType]);
  t["is" + _type] = proxy(t["is" + newType]);
  t["assert" + _type] = proxy(t["assert" + newType]);
};

for (var _type in t.DEPRECATED_KEYS) {
  _loop(_type);
}

/**
 * Description
 */

function validate(node, key, val) {
  if (!node) return;

  var fields = t.NODE_FIELDS[node.type];
  if (!fields) return;

  var field = fields[key];
  if (!field || !field.validate) return;
  if (field.optional && val == null) return;

  field.validate(node, key, val);
}

/**
 * Test if an object is shallowly equal.
 */

function shallowEqual(actual, expected) {
  var keys = (0, _keys2.default)(expected);

  for (var _iterator3 = keys, _isArray3 = Array.isArray(_iterator3), _i3 = 0, _iterator3 = _isArray3 ? _iterator3 : (0, _getIterator3.default)(_iterator3);;) {
    var _ref3;

    if (_isArray3) {
      if (_i3 >= _iterator3.length) break;
      _ref3 = _iterator3[_i3++];
    } else {
      _i3 = _iterator3.next();
      if (_i3.done) break;
      _ref3 = _i3.value;
    }

    var key = _ref3;

    if (actual[key] !== expected[key]) {
      return false;
    }
  }

  return true;
}

/**
 * Append a node to a member expression.
 */

function appendToMemberExpression(member, append, computed) {
  member.object = t.memberExpression(member.object, member.property, member.computed);
  member.property = append;
  member.computed = !!computed;
  return member;
}

/**
 * Prepend a node to a member expression.
 */

function prependToMemberExpression(member, prepend) {
  member.object = t.memberExpression(prepend, member.object);
  return member;
}

/**
 * Ensure the `key` (defaults to "body") of a `node` is a block.
 * Casting it to a block if it is not.
 */

function ensureBlock(node) {
  var key = arguments.length <= 1 || arguments[1] === undefined ? "body" : arguments[1];

  return node[key] = t.toBlock(node[key], node);
}

/**
 * Create a shallow clone of a `node` excluding `_private` properties.
 */

function clone(node) {
  var newNode = {};
  for (var key in node) {
    if (key[0] === "_") continue;
    newNode[key] = node[key];
  }
  return newNode;
}

/**
 * Create a shallow clone of a `node` excluding `_private` and location properties.
 */

function cloneWithoutLoc(node) {
  var newNode = clone(node);
  delete newNode.loc;
  return newNode;
}

/**
 * Create a deep clone of a `node` and all of it's child nodes
 * exluding `_private` properties.
 */

function cloneDeep(node) {
  var newNode = {};

  for (var key in node) {
    if (key[0] === "_") continue;

    var val = node[key];

    if (val) {
      if (val.type) {
        val = t.cloneDeep(val);
      } else if (Array.isArray(val)) {
        val = val.map(t.cloneDeep);
      }
    }

    newNode[key] = val;
  }

  return newNode;
}

/**
 * Build a function that when called will return whether or not the
 * input `node` `MemberExpression` matches the input `match`.
 *
 * For example, given the match `React.createClass` it would match the
 * parsed nodes of `React.createClass` and `React["createClass"]`.
 */

function buildMatchMemberExpression(match, allowPartial) {
  var parts = match.split(".");

  return function (member) {
    // not a member expression
    if (!t.isMemberExpression(member)) return false;

    var search = [member];
    var i = 0;

    while (search.length) {
      var node = search.shift();

      if (allowPartial && i === parts.length) {
        return true;
      }

      if (t.isIdentifier(node)) {
        // this part doesn't match
        if (parts[i] !== node.name) return false;
      } else if (t.isStringLiteral(node)) {
        // this part doesn't match
        if (parts[i] !== node.value) return false;
      } else if (t.isMemberExpression(node)) {
        if (node.computed && !t.isStringLiteral(node.property)) {
          // we can't deal with this
          return false;
        } else {
          search.push(node.object);
          search.push(node.property);
          continue;
        }
      } else {
        // we can't deal with this
        return false;
      }

      // too many parts
      if (++i > parts.length) {
        return false;
      }
    }

    return true;
  };
}

/**
 * Remove comment properties from a node.
 */

function removeComments(node) {
  for (var _iterator4 = t.COMMENT_KEYS, _isArray4 = Array.isArray(_iterator4), _i4 = 0, _iterator4 = _isArray4 ? _iterator4 : (0, _getIterator3.default)(_iterator4);;) {
    var _ref4;

    if (_isArray4) {
      if (_i4 >= _iterator4.length) break;
      _ref4 = _iterator4[_i4++];
    } else {
      _i4 = _iterator4.next();
      if (_i4.done) break;
      _ref4 = _i4.value;
    }

    var key = _ref4;

    delete node[key];
  }
  return node;
}

/**
 * Inherit all unique comments from `parent` node to `child` node.
 */

function inheritsComments(child, parent) {
  inheritTrailingComments(child, parent);
  inheritLeadingComments(child, parent);
  inheritInnerComments(child, parent);
  return child;
}

function inheritTrailingComments(child, parent) {
  _inheritComments("trailingComments", child, parent);
}

function inheritLeadingComments(child, parent) {
  _inheritComments("leadingComments", child, parent);
}

function inheritInnerComments(child, parent) {
  _inheritComments("innerComments", child, parent);
}

function _inheritComments(key, child, parent) {
  if (child && parent) {
    child[key] = (0, _uniq2.default)((0, _compact2.default)([].concat(child[key], parent[key])));
  }
}

// Can't use import because of cyclic dependency between babel-traverse
// and this module (babel-types). This require needs to appear after
// we export the TYPES constant, so we lazy-initialize it before use.
var traverse = void 0;

/**
 * Inherit all contextual properties from `parent` node to `child` node.
 */

function inherits(child, parent) {
  if (!traverse) traverse = require("babel-traverse").default;

  if (!child || !parent) return child;

  // optionally inherit specific properties if not null
  for (var _iterator5 = t.INHERIT_KEYS.optional, _isArray5 = Array.isArray(_iterator5), _i5 = 0, _iterator5 = _isArray5 ? _iterator5 : (0, _getIterator3.default)(_iterator5);;) {
    var _ref5;

    if (_isArray5) {
      if (_i5 >= _iterator5.length) break;
      _ref5 = _iterator5[_i5++];
    } else {
      _i5 = _iterator5.next();
      if (_i5.done) break;
      _ref5 = _i5.value;
    }

    var _key2 = _ref5;

    if (child[_key2] == null) {
      child[_key2] = parent[_key2];
    }
  }

  // force inherit "private" properties
  for (var key in parent) {
    if (key[0] === "_") child[key] = parent[key];
  }

  // force inherit select properties
  for (var _iterator6 = t.INHERIT_KEYS.force, _isArray6 = Array.isArray(_iterator6), _i6 = 0, _iterator6 = _isArray6 ? _iterator6 : (0, _getIterator3.default)(_iterator6);;) {
    var _ref6;

    if (_isArray6) {
      if (_i6 >= _iterator6.length) break;
      _ref6 = _iterator6[_i6++];
    } else {
      _i6 = _iterator6.next();
      if (_i6.done) break;
      _ref6 = _i6.value;
    }

    var _key3 = _ref6;

    child[_key3] = parent[_key3];
  }

  t.inheritsComments(child, parent);
  traverse.copyCache(parent, child);

  return child;
}

/**
 * TODO
 */

function assertNode(node) {
  if (!isNode(node)) {
    // $FlowFixMe
    throw new TypeError("Not a valid node " + (node && node.type));
  }
}

/**
 * TODO
 */

function isNode(node) {
  return !!(node && _definitions.VISITOR_KEYS[node.type]);
}

// Optimize property access.
(0, _toFastProperties2.default)(t);
(0, _toFastProperties2.default)(t.VISITOR_KEYS);

//