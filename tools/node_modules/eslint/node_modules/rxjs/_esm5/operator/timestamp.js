/** PURE_IMPORTS_START .._scheduler_async,.._operators_timestamp PURE_IMPORTS_END */
import { async } from '../scheduler/async';
import { timestamp as higherOrder } from '../operators/timestamp';
/**
 * @param scheduler
 * @return {Observable<Timestamp<any>>|WebSocketSubject<T>|Observable<T>}
 * @method timestamp
 * @owner Observable
 */
export function timestamp(scheduler) {
    if (scheduler === void 0) {
        scheduler = async;
    }
    return higherOrder(scheduler)(this);
}
//# sourceMappingURL=timestamp.js.map
