/** PURE_IMPORTS_START .._operators_subscribeOn PURE_IMPORTS_END */
import { subscribeOn as higherOrder } from '../operators/subscribeOn';
/**
 * Asynchronously subscribes Observers to this Observable on the specified IScheduler.
 *
 * <img src="./img/subscribeOn.png" width="100%">
 *
 * @param {Scheduler} scheduler - The IScheduler to perform subscription actions on.
 * @return {Observable<T>} The source Observable modified so that its subscriptions happen on the specified IScheduler.
 .
 * @method subscribeOn
 * @owner Observable
 */
export function subscribeOn(scheduler, delay) {
    if (delay === void 0) {
        delay = 0;
    }
    return higherOrder(scheduler, delay)(this);
}
//# sourceMappingURL=subscribeOn.js.map
