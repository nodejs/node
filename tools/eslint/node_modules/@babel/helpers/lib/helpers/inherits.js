"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = _inherits;
var _setPrototypeOf = require("setPrototypeOf");
function _inherits(subClass, superClass) {
  if (typeof superClass !== "function" && superClass !== null) {
    throw new TypeError("Super expression must either be null or a function");
  }
  subClass.prototype = Object.create(superClass && superClass.prototype, {
    constructor: {
      value: subClass,
      writable: true,
      configurable: true
    }
  });
  Object.defineProperty(subClass, "prototype", {
    writable: false
  });
  if (superClass) _setPrototypeOf(subClass, superClass);
}

//# sourceMappingURL=inherits.js.map
