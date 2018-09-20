"use strict";

exports.__esModule = true;
exports.default = builder;

var _clone = _interopRequireDefault(require("lodash/clone"));

var _definitions = require("../definitions");

var _validate = _interopRequireDefault(require("../validators/validate"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function builder(type) {
  for (var _len = arguments.length, args = new Array(_len > 1 ? _len - 1 : 0), _key = 1; _key < _len; _key++) {
    args[_key - 1] = arguments[_key];
  }

  var keys = _definitions.BUILDER_KEYS[type];
  var countArgs = args.length;

  if (countArgs > keys.length) {
    throw new Error(type + ": Too many arguments passed. Received " + countArgs + " but can receive no more than " + keys.length);
  }

  var node = {
    type: type
  };
  var i = 0;
  keys.forEach(function (key) {
    var field = _definitions.NODE_FIELDS[type][key];
    var arg;
    if (i < countArgs) arg = args[i];
    if (arg === undefined) arg = (0, _clone.default)(field.default);
    node[key] = arg;
    i++;
  });

  for (var key in node) {
    (0, _validate.default)(node, key, node[key]);
  }

  return node;
}