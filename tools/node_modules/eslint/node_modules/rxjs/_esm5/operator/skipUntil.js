/** PURE_IMPORTS_START .._operators_skipUntil PURE_IMPORTS_END */
import { skipUntil as higherOrder } from '../operators/skipUntil';
/**
 * Returns an Observable that skips items emitted by the source Observable until a second Observable emits an item.
 *
 * <img src="./img/skipUntil.png" width="100%">
 *
 * @param {Observable} notifier - The second Observable that has to emit an item before the source Observable's elements begin to
 * be mirrored by the resulting Observable.
 * @return {Observable<T>} An Observable that skips items from the source Observable until the second Observable emits
 * an item, then emits the remaining items.
 * @method skipUntil
 * @owner Observable
 */
export function skipUntil(notifier) {
    return higherOrder(notifier)(this);
}
//# sourceMappingURL=skipUntil.js.map
