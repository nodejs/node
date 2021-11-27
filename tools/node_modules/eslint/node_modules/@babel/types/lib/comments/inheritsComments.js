"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = inheritsComments;

var _inheritTrailingComments = require("./inheritTrailingComments");

var _inheritLeadingComments = require("./inheritLeadingComments");

var _inheritInnerComments = require("./inheritInnerComments");

function inheritsComments(child, parent) {
  (0, _inheritTrailingComments.default)(child, parent);
  (0, _inheritLeadingComments.default)(child, parent);
  (0, _inheritInnerComments.default)(child, parent);
  return child;
}