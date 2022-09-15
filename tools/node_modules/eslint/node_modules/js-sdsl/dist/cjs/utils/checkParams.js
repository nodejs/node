"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.checkWithinAccessParams = void 0;
/**
 * @description Check if access is out of bounds.
 * @param pos The position want to access.
 * @param lower The lower bound.
 * @param upper The upper bound.
 * @return Boolean about if access is out of bounds.
 */
function checkWithinAccessParams(pos, lower, upper) {
    if (pos < lower || pos > upper) {
        throw new RangeError();
    }
}
exports.checkWithinAccessParams = checkWithinAccessParams;
