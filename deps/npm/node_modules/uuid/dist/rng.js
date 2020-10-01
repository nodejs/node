"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = rng;

var _crypto = _interopRequireDefault(require("crypto"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

const rnds8 = new Uint8Array(16);

function rng() {
  return _crypto.default.randomFillSync(rnds8);
}