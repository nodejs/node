import { Observable } from '../Observable';
import { IScheduler } from '../Scheduler';
/**
 * @method shareReplay
 * @owner Observable
 */
export declare function shareReplay<T>(this: Observable<T>, bufferSize?: number, windowTime?: number, scheduler?: IScheduler): Observable<T>;
