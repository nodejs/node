"use strict";

exports.__esModule = true;
exports.assertEach = assertEach;
exports.assertOneOf = assertOneOf;
exports.assertNodeType = assertNodeType;
exports.assertNodeOrValueType = assertNodeOrValueType;
exports.assertValueType = assertValueType;
exports.chain = chain;
exports.default = defineType;
exports.DEPRECATED_KEYS = exports.BUILDER_KEYS = exports.NODE_FIELDS = exports.FLIPPED_ALIAS_KEYS = exports.ALIAS_KEYS = exports.VISITOR_KEYS = void 0;

var _is = _interopRequireDefault(require("../validators/is"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

var VISITOR_KEYS = {};
exports.VISITOR_KEYS = VISITOR_KEYS;
var ALIAS_KEYS = {};
exports.ALIAS_KEYS = ALIAS_KEYS;
var FLIPPED_ALIAS_KEYS = {};
exports.FLIPPED_ALIAS_KEYS = FLIPPED_ALIAS_KEYS;
var NODE_FIELDS = {};
exports.NODE_FIELDS = NODE_FIELDS;
var BUILDER_KEYS = {};
exports.BUILDER_KEYS = BUILDER_KEYS;
var DEPRECATED_KEYS = {};
exports.DEPRECATED_KEYS = DEPRECATED_KEYS;

function getType(val) {
  if (Array.isArray(val)) {
    return "array";
  } else if (val === null) {
    return "null";
  } else if (val === undefined) {
    return "undefined";
  } else {
    return typeof val;
  }
}

function assertEach(callback) {
  function validator(node, key, val) {
    if (!Array.isArray(val)) return;

    for (var i = 0; i < val.length; i++) {
      callback(node, key + "[" + i + "]", val[i]);
    }
  }

  validator.each = callback;
  return validator;
}

function assertOneOf() {
  for (var _len = arguments.length, values = new Array(_len), _key = 0; _key < _len; _key++) {
    values[_key] = arguments[_key];
  }

  function validate(node, key, val) {
    if (values.indexOf(val) < 0) {
      throw new TypeError("Property " + key + " expected value to be one of " + JSON.stringify(values) + " but got " + JSON.stringify(val));
    }
  }

  validate.oneOf = values;
  return validate;
}

function assertNodeType() {
  for (var _len2 = arguments.length, types = new Array(_len2), _key2 = 0; _key2 < _len2; _key2++) {
    types[_key2] = arguments[_key2];
  }

  function validate(node, key, val) {
    var valid = false;

    for (var _i = 0; _i < types.length; _i++) {
      var type = types[_i];

      if ((0, _is.default)(type, val)) {
        valid = true;
        break;
      }
    }

    if (!valid) {
      throw new TypeError("Property " + key + " of " + node.type + " expected node to be of a type " + JSON.stringify(types) + " " + ("but instead got " + JSON.stringify(val && val.type)));
    }
  }

  validate.oneOfNodeTypes = types;
  return validate;
}

function assertNodeOrValueType() {
  for (var _len3 = arguments.length, types = new Array(_len3), _key3 = 0; _key3 < _len3; _key3++) {
    types[_key3] = arguments[_key3];
  }

  function validate(node, key, val) {
    var valid = false;

    for (var _i2 = 0; _i2 < types.length; _i2++) {
      var type = types[_i2];

      if (getType(val) === type || (0, _is.default)(type, val)) {
        valid = true;
        break;
      }
    }

    if (!valid) {
      throw new TypeError("Property " + key + " of " + node.type + " expected node to be of a type " + JSON.stringify(types) + " " + ("but instead got " + JSON.stringify(val && val.type)));
    }
  }

  validate.oneOfNodeOrValueTypes = types;
  return validate;
}

function assertValueType(type) {
  function validate(node, key, val) {
    var valid = getType(val) === type;

    if (!valid) {
      throw new TypeError("Property " + key + " expected type of " + type + " but got " + getType(val));
    }
  }

  validate.type = type;
  return validate;
}

function chain() {
  for (var _len4 = arguments.length, fns = new Array(_len4), _key4 = 0; _key4 < _len4; _key4++) {
    fns[_key4] = arguments[_key4];
  }

  function validate() {
    for (var _i3 = 0; _i3 < fns.length; _i3++) {
      var fn = fns[_i3];
      fn.apply(void 0, arguments);
    }
  }

  validate.chainOf = fns;
  return validate;
}

function defineType(type, opts) {
  if (opts === void 0) {
    opts = {};
  }

  var inherits = opts.inherits && store[opts.inherits] || {};
  var fields = opts.fields || inherits.fields || {};
  var visitor = opts.visitor || inherits.visitor || [];
  var aliases = opts.aliases || inherits.aliases || [];
  var builder = opts.builder || inherits.builder || opts.visitor || [];

  if (opts.deprecatedAlias) {
    DEPRECATED_KEYS[opts.deprecatedAlias] = type;
  }

  var _arr = visitor.concat(builder);

  for (var _i4 = 0; _i4 < _arr.length; _i4++) {
    var key = _arr[_i4];
    fields[key] = fields[key] || {};
  }

  for (var _key5 in fields) {
    var field = fields[_key5];

    if (builder.indexOf(_key5) === -1) {
      field.optional = true;
    }

    if (field.default === undefined) {
      field.default = null;
    } else if (!field.validate) {
      field.validate = assertValueType(getType(field.default));
    }
  }

  VISITOR_KEYS[type] = opts.visitor = visitor;
  BUILDER_KEYS[type] = opts.builder = builder;
  NODE_FIELDS[type] = opts.fields = fields;
  ALIAS_KEYS[type] = opts.aliases = aliases;
  aliases.forEach(function (alias) {
    FLIPPED_ALIAS_KEYS[alias] = FLIPPED_ALIAS_KEYS[alias] || [];
    FLIPPED_ALIAS_KEYS[alias].push(type);
  });
  store[type] = opts;
}

var store = {};