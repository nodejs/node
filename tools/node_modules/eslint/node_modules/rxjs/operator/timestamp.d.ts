import { Observable } from '../Observable';
import { IScheduler } from '../Scheduler';
import { Timestamp } from '../operators/timestamp';
/**
 * @param scheduler
 * @return {Observable<Timestamp<any>>|WebSocketSubject<T>|Observable<T>}
 * @method timestamp
 * @owner Observable
 */
export declare function timestamp<T>(this: Observable<T>, scheduler?: IScheduler): Observable<Timestamp<T>>;
