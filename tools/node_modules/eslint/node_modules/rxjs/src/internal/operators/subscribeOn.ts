import { Operator } from '../Operator';
import { Subscriber } from '../Subscriber';
import { Observable } from '../Observable';
import { SubscribeOnObservable } from '../observable/SubscribeOnObservable';
import { MonoTypeOperatorFunction, SchedulerLike, TeardownLogic } from '../types';

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
export function subscribeOn<T>(scheduler: SchedulerLike, delay: number = 0): MonoTypeOperatorFunction<T> {
  return function subscribeOnOperatorFunction(source: Observable<T>): Observable<T> {
    return source.lift(new SubscribeOnOperator<T>(scheduler, delay));
  };
}

class SubscribeOnOperator<T> implements Operator<T, T> {
  constructor(private scheduler: SchedulerLike,
              private delay: number) {
  }
  call(subscriber: Subscriber<T>, source: any): TeardownLogic {
    return new SubscribeOnObservable<T>(
      source, this.delay, this.scheduler
    ).subscribe(subscriber);
  }
}
