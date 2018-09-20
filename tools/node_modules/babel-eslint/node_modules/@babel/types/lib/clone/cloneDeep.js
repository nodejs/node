"use strict";

exports.__esModule = true;
exports.default = cloneDeep;

function cloneDeep(node) {
  if (!node) return node;
  var newNode = {};
  Object.keys(node).forEach(function (key) {
    if (key[0] === "_") return;
    var val = node[key];

    if (val) {
      if (val.type) {
        val = cloneDeep(val);
      } else if (Array.isArray(val)) {
        val = val.map(cloneDeep);
      }
    }

    newNode[key] = val;
  });
  return newNode;
}