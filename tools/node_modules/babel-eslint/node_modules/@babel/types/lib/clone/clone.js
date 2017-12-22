"use strict";

exports.__esModule = true;
exports.default = clone;

function clone(node) {
  if (!node) return node;
  var newNode = {};
  Object.keys(node).forEach(function (key) {
    if (key[0] === "_") return;
    newNode[key] = node[key];
  });
  return newNode;
}