"use strict";

exports.__esModule = true;
exports.default = validate;

var _definitions = require("../definitions");

function validate(node, key, val) {
  if (!node) return;
  var fields = _definitions.NODE_FIELDS[node.type];
  if (!fields) return;
  var field = fields[key];
  if (!field || !field.validate) return;
  if (field.optional && val == null) return;
  field.validate(node, key, val);
}