"use strict";

exports.__esModule = true;
exports.default = traverseFast;

var _definitions = require("../definitions");

function traverseFast(node, enter, opts) {
  if (!node) return;
  var keys = _definitions.VISITOR_KEYS[node.type];
  if (!keys) return;
  opts = opts || {};
  enter(node, opts);

  for (var _iterator = keys, _isArray = Array.isArray(_iterator), _i = 0, _iterator = _isArray ? _iterator : _iterator[Symbol.iterator]();;) {
    var _ref;

    if (_isArray) {
      if (_i >= _iterator.length) break;
      _ref = _iterator[_i++];
    } else {
      _i = _iterator.next();
      if (_i.done) break;
      _ref = _i.value;
    }

    var _key = _ref;
    var subNode = node[_key];

    if (Array.isArray(subNode)) {
      for (var _iterator2 = subNode, _isArray2 = Array.isArray(_iterator2), _i2 = 0, _iterator2 = _isArray2 ? _iterator2 : _iterator2[Symbol.iterator]();;) {
        var _ref2;

        if (_isArray2) {
          if (_i2 >= _iterator2.length) break;
          _ref2 = _iterator2[_i2++];
        } else {
          _i2 = _iterator2.next();
          if (_i2.done) break;
          _ref2 = _i2.value;
        }

        var _node2 = _ref2;
        traverseFast(_node2, enter, opts);
      }
    } else {
      traverseFast(subNode, enter, opts);
    }
  }
}