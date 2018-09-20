"use strict";

exports.__esModule = true;
exports.default = toKeyAlias;

var _generated = require("../validators/generated");

var _cloneDeep = _interopRequireDefault(require("../clone/cloneDeep"));

var _removePropertiesDeep = _interopRequireDefault(require("../modifications/removePropertiesDeep"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function toKeyAlias(node, key) {
  if (key === void 0) {
    key = node.key;
  }

  var alias;

  if (node.kind === "method") {
    return toKeyAlias.increment() + "";
  } else if ((0, _generated.isIdentifier)(key)) {
    alias = key.name;
  } else if ((0, _generated.isStringLiteral)(key)) {
    alias = JSON.stringify(key.value);
  } else {
    alias = JSON.stringify((0, _removePropertiesDeep.default)((0, _cloneDeep.default)(key)));
  }

  if (node.computed) {
    alias = "[" + alias + "]";
  }

  if (node.static) {
    alias = "static:" + alias;
  }

  return alias;
}

toKeyAlias.uid = 0;

toKeyAlias.increment = function () {
  if (toKeyAlias.uid >= Number.MAX_SAFE_INTEGER) {
    return toKeyAlias.uid = 0;
  } else {
    return toKeyAlias.uid++;
  }
};