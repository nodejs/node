import { Observable } from '../Observable';
import { IScheduler } from '../Scheduler';
import { async } from '../scheduler/async';
import { timeInterval as higherOrder, TimeInterval } from '../operators/timeInterval';
export {TimeInterval};

/**
 * @param scheduler
 * @return {Observable<TimeInterval<any>>|WebSocketSubject<T>|Observable<T>}
 * @method timeInterval
 * @owner Observable
 */
export function timeInterval<T>(this: Observable<T>, scheduler: IScheduler = async): Observable<TimeInterval<T>> {
  return higherOrder(scheduler)(this) as Observable<TimeInterval<T>>;
}
