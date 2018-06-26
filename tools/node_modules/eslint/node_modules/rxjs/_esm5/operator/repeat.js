/** PURE_IMPORTS_START .._operators_repeat PURE_IMPORTS_END */
import { repeat as higherOrder } from '../operators/repeat';
/**
 * Returns an Observable that repeats the stream of items emitted by the source Observable at most count times.
 *
 * <img src="./img/repeat.png" width="100%">
 *
 * @param {number} [count] The number of times the source Observable items are repeated, a count of 0 will yield
 * an empty Observable.
 * @return {Observable} An Observable that repeats the stream of items emitted by the source Observable at most
 * count times.
 * @method repeat
 * @owner Observable
 */
export function repeat(count) {
    if (count === void 0) {
        count = -1;
    }
    return higherOrder(count)(this);
}
//# sourceMappingURL=repeat.js.map
