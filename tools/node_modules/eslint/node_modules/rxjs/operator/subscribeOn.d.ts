import { IScheduler } from '../Scheduler';
import { Observable } from '../Observable';
/**
 * Asynchronously subscribes Observers to this Observable on the specified IScheduler.
 *
 * <img src="./img/subscribeOn.png" width="100%">
 *
 * @param {Scheduler} scheduler - The IScheduler to perform subscription actions on.
 * @return {Observable<T>} The source Observable modified so that its subscriptions happen on the specified IScheduler.
 .
 * @method subscribeOn
 * @owner Observable
 */
export declare function subscribeOn<T>(this: Observable<T>, scheduler: IScheduler, delay?: number): Observable<T>;
