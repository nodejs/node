import { Observable } from '../Observable';
import { isPromise } from '../util/isPromise';
import { isArrayLike } from '../util/isArrayLike';
import { isInteropObservable } from '../util/isInteropObservable';
import { isIterable } from '../util/isIterable';
import { fromArray } from './fromArray';
import { fromPromise } from './fromPromise';
import { fromIterable } from './fromIterable';
import { fromObservable } from './fromObservable';
import { subscribeTo } from '../util/subscribeTo';
import { ObservableInput, SchedulerLike } from '../types';

export function from<T>(input: ObservableInput<T>, scheduler?: SchedulerLike): Observable<T>;
export function from<T>(input: ObservableInput<ObservableInput<T>>, scheduler?: SchedulerLike): Observable<Observable<T>>;

/**
 * Creates an Observable from an Array, an array-like object, a Promise, an iterable object, or an Observable-like object.
 *
 * <span class="informal">Converts almost anything to an Observable.</span>
 *
 * ![](from.png)
 *
 * `from` converts various other objects and data types into Observables. It also converts a Promise, an array-like, or an
 * <a href="https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Iteration_protocols#iterable" target="_blank">iterable</a>
 * object into an Observable that emits the items in that promise, array, or iterable. A String, in this context, is treated
 * as an array of characters. Observable-like objects (contains a function named with the ES2015 Symbol for Observable) can also be
 * converted through this operator.
 *
 * ## Examples
 * ### Converts an array to an Observable
 * ```javascript
 * import { from } from 'rxjs/observable/from';
 *
 * const array = [10, 20, 30];
 * const result = from(array);
 *
 * result.subscribe(x => console.log(x));
 *
 * // Logs:
 * // 10 20 30
 * ```
 *
 * ---
 *
 * ### Convert an infinite iterable (from a generator) to an Observable
 * ```javascript
 * import { take } from 'rxjs/operators';
 * import { from } from 'rxjs/observable/from';
 *
 * function* generateDoubles(seed) {
 *    let i = seed;
 *    while (true) {
 *      yield i;
 *      i = 2 * i; // double it
 *    }
 * }
 *
 * const iterator = generateDoubles(3);
 * const result = from(iterator).pipe(take(10));
 *
 * result.subscribe(x => console.log(x));
 *
 * // Logs:
 * // 3 6 12 24 48 96 192 384 768 1536
 * ```
 *
 * ---
 *
 * ### with async scheduler
 * ```javascript
 * import { from } from 'rxjs/observable/from';
 * import { async } from 'rxjs/scheduler/async';
 *
 * console.log('start');
 *
 * const array = [10, 20, 30];
 * const result = from(array, async);
 *
 * result.subscribe(x => console.log(x));
 *
 * console.log('end');
 *
 * // Logs:
 * // start end 10 20 30
 * ```
 *
 * @see {@link fromEvent}
 * @see {@link fromEventPattern}
 * @see {@link fromPromise}
 *
 * @param {ObservableInput<T>} A subscription object, a Promise, an Observable-like,
 * an Array, an iterable, or an array-like object to be converted.
 * @param {SchedulerLike} An optional {@link SchedulerLike} on which to schedule the emission of values.
 * @return {Observable<T>}
 * @name from
 * @owner Observable
 */

export function from<T>(input: ObservableInput<T>, scheduler?: SchedulerLike): Observable<T> {
  if (!scheduler) {
    if (input instanceof Observable) {
      return input;
    }
    return new Observable<T>(subscribeTo(input));
  }

  if (input != null) {
    if (isInteropObservable(input)) {
      return fromObservable(input, scheduler);
    } else if (isPromise(input)) {
      return fromPromise(input, scheduler);
    } else if (isArrayLike(input)) {
      return fromArray(input, scheduler);
    }  else if (isIterable(input) || typeof input === 'string') {
      return fromIterable(input, scheduler);
    }
  }

  throw new TypeError((input !== null && typeof input || input) + ' is not observable');
}
