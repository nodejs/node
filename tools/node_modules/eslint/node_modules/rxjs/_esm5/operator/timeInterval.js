/** PURE_IMPORTS_START .._scheduler_async,.._operators_timeInterval PURE_IMPORTS_END */
import { async } from '../scheduler/async';
import { timeInterval as higherOrder, TimeInterval } from '../operators/timeInterval';
export { TimeInterval };
/**
 * @param scheduler
 * @return {Observable<TimeInterval<any>>|WebSocketSubject<T>|Observable<T>}
 * @method timeInterval
 * @owner Observable
 */
export function timeInterval(scheduler) {
    if (scheduler === void 0) {
        scheduler = async;
    }
    return higherOrder(scheduler)(this);
}
//# sourceMappingURL=timeInterval.js.map
