/** PURE_IMPORTS_START .._operators_skip PURE_IMPORTS_END */
import { skip as higherOrder } from '../operators/skip';
/**
 * Returns an Observable that skips the first `count` items emitted by the source Observable.
 *
 * <img src="./img/skip.png" width="100%">
 *
 * @param {Number} count - The number of times, items emitted by source Observable should be skipped.
 * @return {Observable} An Observable that skips values emitted by the source Observable.
 *
 * @method skip
 * @owner Observable
 */
export function skip(count) {
    return higherOrder(count)(this);
}
//# sourceMappingURL=skip.js.map
