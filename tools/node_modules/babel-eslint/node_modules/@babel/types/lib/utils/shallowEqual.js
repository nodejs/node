"use strict";

exports.__esModule = true;
exports.default = shallowEqual;

function shallowEqual(actual, expected) {
  var keys = Object.keys(expected);
  var _arr = keys;

  for (var _i = 0; _i < _arr.length; _i++) {
    var key = _arr[_i];

    if (actual[key] !== expected[key]) {
      return false;
    }
  }

  return true;
}