"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = _wrapAsyncGenerator;
var _AsyncGenerator = require("AsyncGenerator");
function _wrapAsyncGenerator(fn) {
  return function () {
    return new _AsyncGenerator(fn.apply(this, arguments));
  };
}

//# sourceMappingURL=wrapAsyncGenerator.js.map
