import { async } from '../scheduler/async';
import { timestamp as higherOrder } from '../operators/timestamp';
/**
 * @param scheduler
 * @return {Observable<Timestamp<any>>|WebSocketSubject<T>|Observable<T>}
 * @method timestamp
 * @owner Observable
 */
export function timestamp(scheduler = async) {
    return higherOrder(scheduler)(this);
}
//# sourceMappingURL=timestamp.js.map