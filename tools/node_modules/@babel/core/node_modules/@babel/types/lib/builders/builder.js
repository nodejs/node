"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = builder;

var _definitions = require("../definitions");

var _validate = require("../validators/validate");

function builder() {
  const type = this;
  const keys = _definitions.BUILDER_KEYS[type];
  const countArgs = arguments.length;

  if (countArgs > keys.length) {
    throw new Error(`${type}: Too many arguments passed. Received ${countArgs} but can receive no more than ${keys.length}`);
  }

  const node = {
    type
  };

  for (let i = 0; i < keys.length; ++i) {
    const key = keys[i];
    const field = _definitions.NODE_FIELDS[type][key];
    let arg;
    if (i < countArgs) arg = arguments[i];

    if (arg === undefined) {
      arg = Array.isArray(field.default) ? [] : field.default;
    }

    node[key] = arg;
  }

  for (const key in node) {
    (0, _validate.default)(node, key, node[key]);
  }

  return node;
}