import { MonoTypeOperatorFunction, SchedulerLike } from '../types';
/**
 * Asynchronously subscribes Observers to this Observable on the specified {@link SchedulerLike}.
 *
 * ![](subscribeOn.png)
 *
 * @param {SchedulerLike} scheduler - The {@link SchedulerLike} to perform subscription actions on.
 * @return {Observable<T>} The source Observable modified so that its subscriptions happen on the specified {@link SchedulerLike}.
 .
 * @method subscribeOn
 * @owner Observable
 */
export declare function subscribeOn<T>(scheduler: SchedulerLike, delay?: number): MonoTypeOperatorFunction<T>;
