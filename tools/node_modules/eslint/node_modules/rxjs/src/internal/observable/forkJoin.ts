import { Observable } from '../Observable';
import { ObservableInput } from '../types';
import { isArray } from '../util/isArray';
import { EMPTY } from './empty';
import { subscribeToResult } from '../util/subscribeToResult';
import { OuterSubscriber } from '../OuterSubscriber';
import { InnerSubscriber } from '../InnerSubscriber';
import { Subscriber } from '../Subscriber';
import { map } from '../operators/map';

/* tslint:disable:max-line-length */
// forkJoin([a$, b$, c$]);
export function forkJoin<T>(sources: [ObservableInput<T>]): Observable<T[]>;
export function forkJoin<T, T2>(sources: [ObservableInput<T>, ObservableInput<T2>]): Observable<[T, T2]>;
export function forkJoin<T, T2, T3>(sources: [ObservableInput<T>, ObservableInput<T2>, ObservableInput<T3>]): Observable<[T, T2, T3]>;
export function forkJoin<T, T2, T3, T4>(sources: [ObservableInput<T>, ObservableInput<T2>, ObservableInput<T3>, ObservableInput<T4>]): Observable<[T, T2, T3, T4]>;
export function forkJoin<T, T2, T3, T4, T5>(sources: [ObservableInput<T>, ObservableInput<T2>, ObservableInput<T3>, ObservableInput<T4>, ObservableInput<T5>]): Observable<[T, T2, T3, T4, T5]>;
export function forkJoin<T, T2, T3, T4, T5, T6>(sources: [ObservableInput<T>, ObservableInput<T2>, ObservableInput<T3>, ObservableInput<T4>, ObservableInput<T5>, ObservableInput<T6>]): Observable<[T, T2, T3, T4, T5, T6]>;
export function forkJoin<T>(sources: Array<ObservableInput<T>>): Observable<T[]>;

// forkJoin(a$, b$, c$)
export function forkJoin<T>(v1: ObservableInput<T>): Observable<T[]>;
export function forkJoin<T, T2>(v1: ObservableInput<T>, v2: ObservableInput<T2>): Observable<[T, T2]>;
export function forkJoin<T, T2, T3>(v1: ObservableInput<T>, v2: ObservableInput<T2>, v3: ObservableInput<T3>): Observable<[T, T2, T3]>;
export function forkJoin<T, T2, T3, T4>(v1: ObservableInput<T>, v2: ObservableInput<T2>, v3: ObservableInput<T3>, v4: ObservableInput<T4>): Observable<[T, T2, T3, T4]>;
export function forkJoin<T, T2, T3, T4, T5>(v1: ObservableInput<T>, v2: ObservableInput<T2>, v3: ObservableInput<T3>, v4: ObservableInput<T4>, v5: ObservableInput<T5>): Observable<[T, T2, T3, T4, T5]>;
export function forkJoin<T, T2, T3, T4, T5, T6>(v1: ObservableInput<T>, v2: ObservableInput<T2>, v3: ObservableInput<T3>, v4: ObservableInput<T4>, v5: ObservableInput<T5>, v6: ObservableInput<T6>): Observable<[T, T2, T3, T4, T5, T6]>;

/** @deprecated resultSelector is deprecated, pipe to map instead */
export function forkJoin(...args: Array<ObservableInput<any>|Function>): Observable<any>;
export function forkJoin<T>(...sources: ObservableInput<T>[]): Observable<T[]>;
/* tslint:enable:max-line-length */

/**
 * Joins last values emitted by passed Observables.
 *
 * <span class="informal">Wait for Observables to complete and then combine last values they emitted.</span>
 *
 * ![](forkJoin.png)
 *
 * `forkJoin` is an operator that takes any number of Observables which can be passed either as an array
 * or directly as arguments. If no input Observables are provided, resulting stream will complete
 * immediately.
 *
 * `forkJoin` will wait for all passed Observables to complete and then it will emit an array with last
 * values from corresponding Observables. So if you pass `n` Observables to the operator, resulting
 * array will have `n` values, where first value is the last thing emitted by the first Observable,
 * second value is the last thing emitted by the second Observable and so on. That means `forkJoin` will
 * not emit more than once and it will complete after that. If you need to emit combined values not only
 * at the end of lifecycle of passed Observables, but also throughout it, try out {@link combineLatest}
 * or {@link zip} instead.
 *
 * In order for resulting array to have the same length as the number of input Observables, whenever any of
 * that Observables completes without emitting any value, `forkJoin` will complete at that moment as well
 * and it will not emit anything either, even if it already has some last values from other Observables.
 * Conversely, if there is an Observable that never completes, `forkJoin` will never complete as well,
 * unless at any point some other Observable completes without emitting value, which brings us back to
 * the previous case. Overall, in order for `forkJoin` to emit a value, all Observables passed as arguments
 * have to emit something at least once and complete.
 *
 * If any input Observable errors at some point, `forkJoin` will error as well and all other Observables
 * will be immediately unsubscribed.
 *
 * Optionally `forkJoin` accepts project function, that will be called with values which normally
 * would land in emitted array. Whatever is returned by project function, will appear in output
 * Observable instead. This means that default project can be thought of as a function that takes
 * all its arguments and puts them into an array. Note that project function will be called only
 * when output Observable is supposed to emit a result.
 *
 * ## Examples
 * ### Use forkJoin with operator emitting immediately
 * ```javascript
 * import { forkJoin, of } from 'rxjs';
 *
 * const observable = forkJoin(
 *   of(1, 2, 3, 4),
 *   of(5, 6, 7, 8),
 * );
 * observable.subscribe(
 *   value => console.log(value),
 *   err => {},
 *   () => console.log('This is how it ends!'),
 * );
 *
 * // Logs:
 * // [4, 8]
 * // "This is how it ends!"
 * ```
 *
 * ### Use forkJoin with operator emitting after some time
 * ```javascript
 * import { forkJoin, interval } from 'rxjs';
 * import { take } from 'rxjs/operators';
 *
 * const observable = forkJoin(
 *   interval(1000).pipe(take(3)), // emit 0, 1, 2 every second and complete
 *   interval(500).pipe(take(4)),  // emit 0, 1, 2, 3 every half a second and complete
 * );
 * observable.subscribe(
 *   value => console.log(value),
 *   err => {},
 *   () => console.log('This is how it ends!'),
 * );
 *
 * // Logs:
 * // [2, 3] after 3 seconds
 * // "This is how it ends!" immediately after
 * ```
 *
 * ### Use forkJoin with project function
 * ```javascript
 * import { forkJoin, interval } from 'rxjs';
 * import { take } from 'rxjs/operators';
 *
 * const observable = forkJoin(
 *   interval(1000).pipe(take(3)), // emit 0, 1, 2 every second and complete
 *   interval(500).pipe(take(4)),  // emit 0, 1, 2, 3 every half a second and complete
 * ).pipe(
 *   map(([n, m]) => n + m),
 * );
 * observable.subscribe(
 *   value => console.log(value),
 *   err => {},
 *   () => console.log('This is how it ends!'),
 * );
 *
 * // Logs:
 * // 5 after 3 seconds
 * // "This is how it ends!" immediately after
 * ```
 *
 * @see {@link combineLatest}
 * @see {@link zip}
 *
 * @param {...ObservableInput} sources Any number of Observables provided either as an array or as an arguments
 * passed directly to the operator.
 * @param {function} [project] Function that takes values emitted by input Observables and returns value
 * that will appear in resulting Observable instead of default array.
 * @return {Observable} Observable emitting either an array of last values emitted by passed Observables
 * or value from project function.
 */
export function forkJoin<T>(
  ...sources: Array<ObservableInput<T> | ObservableInput<T>[] | Function>
): Observable<T[]> {

  let resultSelector: Function;
  if (typeof sources[sources.length - 1] === 'function') {
    // DEPRECATED PATH
    resultSelector = sources.pop() as Function;
  }

  // if the first and only other argument is an array
  // assume it's been called with `forkJoin([obs1, obs2, obs3])`
  if (sources.length === 1 && isArray(sources[0])) {
    sources = sources[0] as Array<ObservableInput<T>>;
  }

  if (sources.length === 0) {
    return EMPTY;
  }

  if (resultSelector) {
    // DEPRECATED PATH
    return forkJoin(sources).pipe(
      map(args => resultSelector(...args))
    );
  }

  return new Observable(subscriber => {
    return new ForkJoinSubscriber(subscriber, sources as Array<ObservableInput<T>>);
  });
}
/**
 * We need this JSDoc comment for affecting ESDoc.
 * @ignore
 * @extends {Ignored}
 */
class ForkJoinSubscriber<T, R> extends OuterSubscriber<T, T> {
  private completed = 0;
  private values: T[];
  private haveValues = 0;

  constructor(destination: Subscriber<R>,
              private sources: Array<ObservableInput<T>>) {
    super(destination);

    const len = sources.length;
    this.values = new Array(len);

    for (let i = 0; i < len; i++) {
      const source = sources[i];
      const innerSubscription = subscribeToResult(this, source, null, i);

      if (innerSubscription) {
        this.add(innerSubscription);
      }
    }
  }

  notifyNext(outerValue: any, innerValue: T,
             outerIndex: number, innerIndex: number,
             innerSub: InnerSubscriber<T, T>): void {
    this.values[outerIndex] = innerValue;
    if (!(innerSub as any)._hasValue) {
      (innerSub as any)._hasValue = true;
      this.haveValues++;
    }
  }

  notifyComplete(innerSub: InnerSubscriber<T, T>): void {
    const { destination, haveValues, values } = this;
    const len = values.length;

    if (!(innerSub as any)._hasValue) {
      destination.complete();
      return;
    }

    this.completed++;

    if (this.completed !== len) {
      return;
    }

    if (haveValues === len) {
      destination.next(values);
    }

    destination.complete();
  }
}
