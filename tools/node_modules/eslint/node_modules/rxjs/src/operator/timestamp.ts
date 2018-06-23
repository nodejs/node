import { Observable } from '../Observable';
import { IScheduler } from '../Scheduler';
import { async } from '../scheduler/async';
import { timestamp as higherOrder } from '../operators/timestamp';
import { Timestamp } from '../operators/timestamp';
/**
 * @param scheduler
 * @return {Observable<Timestamp<any>>|WebSocketSubject<T>|Observable<T>}
 * @method timestamp
 * @owner Observable
 */
export function timestamp<T>(this: Observable<T>, scheduler: IScheduler = async): Observable<Timestamp<T>> {
  return higherOrder(scheduler)(this) as Observable<Timestamp<T>>;
}
