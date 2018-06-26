import { async } from '../scheduler/async';
import { timeInterval as higherOrder, TimeInterval } from '../operators/timeInterval';
export { TimeInterval };
/**
 * @param scheduler
 * @return {Observable<TimeInterval<any>>|WebSocketSubject<T>|Observable<T>}
 * @method timeInterval
 * @owner Observable
 */
export function timeInterval(scheduler = async) {
    return higherOrder(scheduler)(this);
}
//# sourceMappingURL=timeInterval.js.map