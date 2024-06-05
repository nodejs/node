"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = _wrapNativeSuper;
var _getPrototypeOf = require("getPrototypeOf");
var _setPrototypeOf = require("setPrototypeOf");
var _isNativeFunction = require("isNativeFunction");
var _construct = require("construct");
function _wrapNativeSuper(Class) {
  var _cache = typeof Map === "function" ? new Map() : undefined;
  exports.default = _wrapNativeSuper = function _wrapNativeSuper(Class) {
    if (Class === null || !_isNativeFunction(Class)) return Class;
    if (typeof Class !== "function") {
      throw new TypeError("Super expression must either be null or a function");
    }
    if (typeof _cache !== "undefined") {
      if (_cache.has(Class)) return _cache.get(Class);
      _cache.set(Class, Wrapper);
    }
    function Wrapper() {
      return _construct(Class, arguments, _getPrototypeOf(this).constructor);
    }
    Wrapper.prototype = Object.create(Class.prototype, {
      constructor: {
        value: Wrapper,
        enumerable: false,
        writable: true,
        configurable: true
      }
    });
    return _setPrototypeOf(Wrapper, Class);
  };
  return _wrapNativeSuper(Class);
}

//# sourceMappingURL=wrapNativeSuper.js.map
