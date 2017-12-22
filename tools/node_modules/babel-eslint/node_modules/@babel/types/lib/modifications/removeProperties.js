"use strict";

exports.__esModule = true;
exports.default = removeProperties;

var _constants = require("../constants");

var CLEAR_KEYS = ["tokens", "start", "end", "loc", "raw", "rawValue"];

var CLEAR_KEYS_PLUS_COMMENTS = _constants.COMMENT_KEYS.concat(["comments"]).concat(CLEAR_KEYS);

function removeProperties(node, opts) {
  if (opts === void 0) {
    opts = {};
  }

  var map = opts.preserveComments ? CLEAR_KEYS : CLEAR_KEYS_PLUS_COMMENTS;

  for (var _iterator = map, _isArray = Array.isArray(_iterator), _i = 0, _iterator = _isArray ? _iterator : _iterator[Symbol.iterator]();;) {
    var _ref;

    if (_isArray) {
      if (_i >= _iterator.length) break;
      _ref = _iterator[_i++];
    } else {
      _i = _iterator.next();
      if (_i.done) break;
      _ref = _i.value;
    }

    var _key2 = _ref;
    if (node[_key2] != null) node[_key2] = undefined;
  }

  for (var _key in node) {
    if (_key[0] === "_" && node[_key] != null) node[_key] = undefined;
  }

  var symbols = Object.getOwnPropertySymbols(node);

  for (var _iterator2 = symbols, _isArray2 = Array.isArray(_iterator2), _i2 = 0, _iterator2 = _isArray2 ? _iterator2 : _iterator2[Symbol.iterator]();;) {
    var _ref2;

    if (_isArray2) {
      if (_i2 >= _iterator2.length) break;
      _ref2 = _iterator2[_i2++];
    } else {
      _i2 = _iterator2.next();
      if (_i2.done) break;
      _ref2 = _i2.value;
    }

    var _sym = _ref2;
    node[_sym] = null;
  }
}