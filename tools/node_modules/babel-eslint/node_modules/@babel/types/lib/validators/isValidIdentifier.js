"use strict";

exports.__esModule = true;
exports.default = isValidIdentifier;

var _esutils = _interopRequireDefault(require("esutils"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function isValidIdentifier(name) {
  if (typeof name !== "string" || _esutils.default.keyword.isReservedWordES6(name, true)) {
    return false;
  } else if (name === "await") {
    return false;
  } else {
    return _esutils.default.keyword.isIdentifierNameES6(name);
  }
}