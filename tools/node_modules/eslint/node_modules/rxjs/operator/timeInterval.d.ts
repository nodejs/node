import { Observable } from '../Observable';
import { IScheduler } from '../Scheduler';
import { TimeInterval } from '../operators/timeInterval';
export { TimeInterval };
/**
 * @param scheduler
 * @return {Observable<TimeInterval<any>>|WebSocketSubject<T>|Observable<T>}
 * @method timeInterval
 * @owner Observable
 */
export declare function timeInterval<T>(this: Observable<T>, scheduler?: IScheduler): Observable<TimeInterval<T>>;
