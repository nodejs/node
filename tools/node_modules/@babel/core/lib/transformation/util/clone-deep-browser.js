"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = _default;
const serialized = "$$ babel internal serialized type" + Math.random();

function serialize(key, value) {
  if (typeof value !== "bigint") return value;
  return {
    [serialized]: "BigInt",
    value: value.toString()
  };
}

function revive(key, value) {
  if (!value || typeof value !== "object") return value;
  if (value[serialized] !== "BigInt") return value;
  return BigInt(value.value);
}

function _default(value) {
  return JSON.parse(JSON.stringify(value, serialize), revive);
}