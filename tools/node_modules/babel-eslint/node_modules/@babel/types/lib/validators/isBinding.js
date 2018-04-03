"use strict";

exports.__esModule = true;
exports.default = isBinding;

var _getBindingIdentifiers = _interopRequireDefault(require("../retrievers/getBindingIdentifiers"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function isBinding(node, parent) {
  var keys = _getBindingIdentifiers.default.keys[parent.type];

  if (keys) {
    for (var i = 0; i < keys.length; i++) {
      var key = keys[i];
      var val = parent[key];

      if (Array.isArray(val)) {
        if (val.indexOf(node) >= 0) return true;
      } else {
        if (val === node) return true;
      }
    }
  }

  return false;
}