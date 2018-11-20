import { Observable } from '../Observable';
import { Subscription } from '../Subscription';
import { observable as Symbol_observable } from '../symbol/observable';
import { subscribeToObservable } from '../util/subscribeToObservable';
import { InteropObservable, SchedulerLike, Subscribable } from '../types';

export function fromObservable<T>(input: InteropObservable<T>, scheduler: SchedulerLike) {
  if (!scheduler) {
    return new Observable<T>(subscribeToObservable(input));
  } else {
    return new Observable<T>(subscriber => {
      const sub = new Subscription();
      sub.add(scheduler.schedule(() => {
        const observable: Subscribable<T> = input[Symbol_observable]();
        sub.add(observable.subscribe({
          next(value) { sub.add(scheduler.schedule(() => subscriber.next(value))); },
          error(err) { sub.add(scheduler.schedule(() => subscriber.error(err))); },
          complete() { sub.add(scheduler.schedule(() => subscriber.complete())); },
        }));
      }));
      return sub;
    });
  }
}
