import { Observable } from '../Observable';
import { SchedulerLike } from '../types';
import { Subscription } from '../Subscription';
import { iterator as Symbol_iterator } from '../symbol/iterator';
import { subscribeToIterable } from '../util/subscribeToIterable';

export function fromIterable<T>(input: Iterable<T>, scheduler: SchedulerLike) {
  if (!input) {
    throw new Error('Iterable cannot be null');
  }
  if (!scheduler) {
    return new Observable<T>(subscribeToIterable(input));
  } else {
    return new Observable<T>(subscriber => {
      const sub = new Subscription();
      let iterator: Iterator<T>;
      sub.add(() => {
        // Finalize generators
        if (iterator && typeof iterator.return === 'function') {
          iterator.return();
        }
      });
      sub.add(scheduler.schedule(() => {
        iterator = input[Symbol_iterator]();
        sub.add(scheduler.schedule(function () {
          if (subscriber.closed) {
            return;
          }
          let value: T;
          let done: boolean;
          try {
            const result = iterator.next();
            value = result.value;
            done = result.done;
          } catch (err) {
            subscriber.error(err);
            return;
          }
          if (done) {
            subscriber.complete();
          } else {
            subscriber.next(value);
            this.schedule();
          }
        }));
      }));
      return sub;
    });
  }
}
