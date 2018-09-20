"use strict";

exports.__esModule = true;
exports.default = isCompatTag;

function isCompatTag(tagName) {
  return !!tagName && /^[a-z]|-/.test(tagName);
}