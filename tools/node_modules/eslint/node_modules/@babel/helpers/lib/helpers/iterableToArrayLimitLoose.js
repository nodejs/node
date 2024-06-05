"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = _iterableToArrayLimitLoose;
function _iterableToArrayLimitLoose(arr, i) {
  var iterator = arr && (typeof Symbol !== "undefined" && arr[Symbol.iterator] || arr["@@iterator"]);
  if (iterator == null) return;
  var _arr = [];
  var step;
  iterator = iterator.call(arr);
  while (arr.length < i && !(step = iterator.next()).done) {
    _arr.push(step.value);
  }
  return _arr;
}

//# sourceMappingURL=iterableToArrayLimitLoose.js.map
