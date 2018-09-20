"use strict";

exports.__esModule = true;
exports.default = isType;

var _definitions = require("../definitions");

function isType(nodeType, targetType) {
  if (nodeType === targetType) return true;
  if (_definitions.ALIAS_KEYS[targetType]) return false;
  var aliases = _definitions.FLIPPED_ALIAS_KEYS[targetType];

  if (aliases) {
    if (aliases[0] === nodeType) return true;

    for (var _iterator = aliases, _isArray = Array.isArray(_iterator), _i = 0, _iterator = _isArray ? _iterator : _iterator[Symbol.iterator]();;) {
      var _ref;

      if (_isArray) {
        if (_i >= _iterator.length) break;
        _ref = _iterator[_i++];
      } else {
        _i = _iterator.next();
        if (_i.done) break;
        _ref = _i.value;
      }

      var _alias = _ref;
      if (nodeType === _alias) return true;
    }
  }

  return false;
}