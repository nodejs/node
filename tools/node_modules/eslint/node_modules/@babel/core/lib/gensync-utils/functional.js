"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.once = once;
var _async = require("./async.js");
function once(fn) {
  let result;
  let resultP;
  return function* () {
    if (result) return result;
    if (!(yield* (0, _async.isAsync)())) return result = yield* fn();
    if (resultP) return yield* (0, _async.waitFor)(resultP);
    let resolve, reject;
    resultP = new Promise((res, rej) => {
      resolve = res;
      reject = rej;
    });
    try {
      result = yield* fn();
      resultP = null;
      resolve(result);
      return result;
    } catch (error) {
      reject(error);
      throw error;
    }
  };
}
0 && 0;

//# sourceMappingURL=functional.js.map
