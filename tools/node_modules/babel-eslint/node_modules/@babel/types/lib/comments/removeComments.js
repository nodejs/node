"use strict";

exports.__esModule = true;
exports.default = removeComments;

var _constants = require("../constants");

function removeComments(node) {
  _constants.COMMENT_KEYS.forEach(function (key) {
    node[key] = null;
  });

  return node;
}