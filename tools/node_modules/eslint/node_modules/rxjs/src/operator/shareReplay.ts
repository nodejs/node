import { Observable } from '../Observable';
import { IScheduler } from '../Scheduler';
import { shareReplay as higherOrder } from '../operators/shareReplay';

/**
 * @method shareReplay
 * @owner Observable
 */
export function shareReplay<T>(this: Observable<T>, bufferSize?: number, windowTime?: number, scheduler?: IScheduler):
  Observable<T> {
  return higherOrder(bufferSize, windowTime, scheduler)(this) as Observable<T>;
};