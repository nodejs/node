import { Observable } from '../Observable';
import { ObservableInput, SchedulerLike } from '../types';
import { isScheduler  } from '../util/isScheduler';
import { isArray  } from '../util/isArray';
import { Subscriber } from '../Subscriber';
import { OuterSubscriber } from '../OuterSubscriber';
import { Operator } from '../Operator';
import { InnerSubscriber } from '../InnerSubscriber';
import { subscribeToResult } from '../util/subscribeToResult';
import { fromArray } from './fromArray';

const NONE = {};

/* tslint:disable:max-line-length */
/** @deprecated resultSelector no longer supported, pipe to map instead */
export function combineLatest<T, R>(v1: ObservableInput<T>, resultSelector: (v1: T) => R, scheduler?: SchedulerLike): Observable<R>;
/** @deprecated resultSelector no longer supported, pipe to map instead */
export function combineLatest<T, T2, R>(v1: ObservableInput<T>, v2: ObservableInput<T2>, resultSelector: (v1: T, v2: T2) => R, scheduler?: SchedulerLike): Observable<R>;
/** @deprecated resultSelector no longer supported, pipe to map instead */
export function combineLatest<T, T2, T3, R>(v1: ObservableInput<T>, v2: ObservableInput<T2>, v3: ObservableInput<T3>, resultSelector: (v1: T, v2: T2, v3: T3) => R, scheduler?: SchedulerLike): Observable<R>;
/** @deprecated resultSelector no longer supported, pipe to map instead */
export function combineLatest<T, T2, T3, T4, R>(v1: ObservableInput<T>, v2: ObservableInput<T2>, v3: ObservableInput<T3>, v4: ObservableInput<T4>, resultSelector: (v1: T, v2: T2, v3: T3, v4: T4) => R, scheduler?: SchedulerLike): Observable<R>;
/** @deprecated resultSelector no longer supported, pipe to map instead */
export function combineLatest<T, T2, T3, T4, T5, R>(v1: ObservableInput<T>, v2: ObservableInput<T2>, v3: ObservableInput<T3>, v4: ObservableInput<T4>, v5: ObservableInput<T5>, resultSelector: (v1: T, v2: T2, v3: T3, v4: T4, v5: T5) => R, scheduler?: SchedulerLike): Observable<R>;
/** @deprecated resultSelector no longer supported, pipe to map instead */
export function combineLatest<T, T2, T3, T4, T5, T6, R>(v1: ObservableInput<T>, v2: ObservableInput<T2>, v3: ObservableInput<T3>, v4: ObservableInput<T4>, v5: ObservableInput<T5>, v6: ObservableInput<T6>, resultSelector: (v1: T, v2: T2, v3: T3, v4: T4, v5: T5, v6: T6) => R, scheduler?: SchedulerLike): Observable<R>;

export function combineLatest<T, T2>(v1: ObservableInput<T>, v2: ObservableInput<T2>, scheduler?: SchedulerLike): Observable<[T, T2]>;
export function combineLatest<T, T2, T3>(v1: ObservableInput<T>, v2: ObservableInput<T2>, v3: ObservableInput<T3>, scheduler?: SchedulerLike): Observable<[T, T2, T3]>;
export function combineLatest<T, T2, T3, T4>(v1: ObservableInput<T>, v2: ObservableInput<T2>, v3: ObservableInput<T3>, v4: ObservableInput<T4>, scheduler?: SchedulerLike): Observable<[T, T2, T3, T4]>;
export function combineLatest<T, T2, T3, T4, T5>(v1: ObservableInput<T>, v2: ObservableInput<T2>, v3: ObservableInput<T3>, v4: ObservableInput<T4>, v5: ObservableInput<T5>, scheduler?: SchedulerLike): Observable<[T, T2, T3, T4, T5]>;
export function combineLatest<T, T2, T3, T4, T5, T6>(v1: ObservableInput<T>, v2: ObservableInput<T2>, v3: ObservableInput<T3>, v4: ObservableInput<T4>, v5: ObservableInput<T5>, v6: ObservableInput<T6>, scheduler?: SchedulerLike): Observable<[T, T2, T3, T4, T5, T6]>;

export function combineLatest<T>(array: ObservableInput<T>[], scheduler?: SchedulerLike): Observable<T[]>;
export function combineLatest<R>(array: ObservableInput<any>[], scheduler?: SchedulerLike): Observable<R>;
/** @deprecated resultSelector no longer supported, pipe to map instead */
export function combineLatest<T, R>(array: ObservableInput<T>[], resultSelector: (...values: Array<T>) => R, scheduler?: SchedulerLike): Observable<R>;
/** @deprecated resultSelector no longer supported, pipe to map instead */
export function combineLatest<R>(array: ObservableInput<any>[], resultSelector: (...values: Array<any>) => R, scheduler?: SchedulerLike): Observable<R>;
export function combineLatest<T>(...observables: Array<ObservableInput<T> | SchedulerLike>): Observable<T[]>;
export function combineLatest<T, R>(...observables: Array<ObservableInput<T> | ((...values: Array<T>) => R) | SchedulerLike>): Observable<R>;
export function combineLatest<R>(...observables: Array<ObservableInput<any> | ((...values: Array<any>) => R) | SchedulerLike>): Observable<R>;
/* tslint:enable:max-line-length */

/**
 * Combines multiple Observables to create an Observable whose values are
 * calculated from the latest values of each of its input Observables.
 *
 * <span class="informal">Whenever any input Observable emits a value, it
 * computes a formula using the latest values from all the inputs, then emits
 * the output of that formula.</span>
 *
 * ![](combineLatest.png)
 *
 * `combineLatest` combines the values from all the Observables passed as
 * arguments. This is done by subscribing to each Observable in order and,
 * whenever any Observable emits, collecting an array of the most recent
 * values from each Observable. So if you pass `n` Observables to operator,
 * returned Observable will always emit an array of `n` values, in order
 * corresponding to order of passed Observables (value from the first Observable
 * on the first place and so on).
 *
 * Static version of `combineLatest` accepts either an array of Observables
 * or each Observable can be put directly as an argument. Note that array of
 * Observables is good choice, if you don't know beforehand how many Observables
 * you will combine. Passing empty array will result in Observable that
 * completes immediately.
 *
 * To ensure output array has always the same length, `combineLatest` will
 * actually wait for all input Observables to emit at least once,
 * before it starts emitting results. This means if some Observable emits
 * values before other Observables started emitting, all these values but the last
 * will be lost. On the other hand, if some Observable does not emit a value but
 * completes, resulting Observable will complete at the same moment without
 * emitting anything, since it will be now impossible to include value from
 * completed Observable in resulting array. Also, if some input Observable does
 * not emit any value and never completes, `combineLatest` will also never emit
 * and never complete, since, again, it will wait for all streams to emit some
 * value.
 *
 * If at least one Observable was passed to `combineLatest` and all passed Observables
 * emitted something, resulting Observable will complete when all combined
 * streams complete. So even if some Observable completes, result of
 * `combineLatest` will still emit values when other Observables do. In case
 * of completed Observable, its value from now on will always be the last
 * emitted value. On the other hand, if any Observable errors, `combineLatest`
 * will error immediately as well, and all other Observables will be unsubscribed.
 *
 * `combineLatest` accepts as optional parameter `project` function, which takes
 * as arguments all values that would normally be emitted by resulting Observable.
 * `project` can return any kind of value, which will be then emitted by Observable
 * instead of default array. Note that `project` does not take as argument that array
 * of values, but values themselves. That means default `project` can be imagined
 * as function that takes all its arguments and puts them into an array.
 *
 * ## Examples
 * ### Combine two timer Observables
 * ```javascript
 * const firstTimer = timer(0, 1000); // emit 0, 1, 2... after every second, starting from now
 * const secondTimer = timer(500, 1000); // emit 0, 1, 2... after every second, starting 0,5s from now
 * const combinedTimers = combineLatest(firstTimer, secondTimer);
 * combinedTimers.subscribe(value => console.log(value));
 * // Logs
 * // [0, 0] after 0.5s
 * // [1, 0] after 1s
 * // [1, 1] after 1.5s
 * // [2, 1] after 2s
 * ```
 *
 * ### Combine an array of Observables
 * ```javascript
 * const observables = [1, 5, 10].map(
 *   n => of(n).pipe(
 *     delay(n * 1000),   // emit 0 and then emit n after n seconds
 *     startWith(0),
 *   )
 * );
 * const combined = combineLatest(observables);
 * combined.subscribe(value => console.log(value));
 * // Logs
 * // [0, 0, 0] immediately
 * // [1, 0, 0] after 1s
 * // [1, 5, 0] after 5s
 * // [1, 5, 10] after 10s
 * ```
 *
 *
 * ### Use project function to dynamically calculate the Body-Mass Index
 * ```javascript
 * * const weight = of(70, 72, 76, 79, 75);
 * const height = of(1.76, 1.77, 1.78);
 * const bmi = combineLatest(weight, height).pipe(
 *   map(([w, h]) => w / (h * h)),
 * );
 * bmi.subscribe(x => console.log('BMI is ' + x));
 *
 * // With output to console:
 * // BMI is 24.212293388429753
 * // BMI is 23.93948099205209
 * // BMI is 23.671253629592222
 * ```
 *
 * @see {@link combineAll}
 * @see {@link merge}
 * @see {@link withLatestFrom}
 *
 * @param {ObservableInput} observable1 An input Observable to combine with other Observables.
 * @param {ObservableInput} observable2 An input Observable to combine with other Observables.
 * More than one input Observables may be given as arguments
 * or an array of Observables may be given as the first argument.
 * @param {function} [project] An optional function to project the values from
 * the combined latest values into a new value on the output Observable.
 * @param {SchedulerLike} [scheduler=null] The {@link SchedulerLike} to use for subscribing to
 * each input Observable.
 * @return {Observable} An Observable of projected values from the most recent
 * values from each input Observable, or an array of the most recent values from
 * each input Observable.
 */
export function combineLatest<T, R>(...observables: Array<any | ObservableInput<any> |
                                                    Array<ObservableInput<any>> |
                                                    (((...values: Array<any>) => R)) |
                                                    SchedulerLike>): Observable<R> {
  let resultSelector: (...values: Array<any>) => R =  null;
  let scheduler: SchedulerLike = null;

  if (isScheduler(observables[observables.length - 1])) {
    scheduler = <SchedulerLike>observables.pop();
  }

  if (typeof observables[observables.length - 1] === 'function') {
    resultSelector = <(...values: Array<any>) => R>observables.pop();
  }

  // if the first and only other argument besides the resultSelector is an array
  // assume it's been called with `combineLatest([obs1, obs2, obs3], resultSelector)`
  if (observables.length === 1 && isArray(observables[0])) {
    observables = <Array<Observable<any>>>observables[0];
  }

  return fromArray(observables, scheduler).lift(new CombineLatestOperator<T, R>(resultSelector));
}

export class CombineLatestOperator<T, R> implements Operator<T, R> {
  constructor(private resultSelector?: (...values: Array<any>) => R) {
  }

  call(subscriber: Subscriber<R>, source: any): any {
    return source.subscribe(new CombineLatestSubscriber(subscriber, this.resultSelector));
  }
}

/**
 * We need this JSDoc comment for affecting ESDoc.
 * @ignore
 * @extends {Ignored}
 */
export class CombineLatestSubscriber<T, R> extends OuterSubscriber<T, R> {
  private active: number = 0;
  private values: any[] = [];
  private observables: any[] = [];
  private toRespond: number;

  constructor(destination: Subscriber<R>, private resultSelector?: (...values: Array<any>) => R) {
    super(destination);
  }

  protected _next(observable: any) {
    this.values.push(NONE);
    this.observables.push(observable);
  }

  protected _complete() {
    const observables = this.observables;
    const len = observables.length;
    if (len === 0) {
      this.destination.complete();
    } else {
      this.active = len;
      this.toRespond = len;
      for (let i = 0; i < len; i++) {
        const observable = observables[i];
        this.add(subscribeToResult(this, observable, observable, i));
      }
    }
  }

  notifyComplete(unused: Subscriber<R>): void {
    if ((this.active -= 1) === 0) {
      this.destination.complete();
    }
  }

  notifyNext(outerValue: T, innerValue: R,
             outerIndex: number, innerIndex: number,
             innerSub: InnerSubscriber<T, R>): void {
    const values = this.values;
    const oldVal = values[outerIndex];
    const toRespond = !this.toRespond
      ? 0
      : oldVal === NONE ? --this.toRespond : this.toRespond;
    values[outerIndex] = innerValue;

    if (toRespond === 0) {
      if (this.resultSelector) {
        this._tryResultSelector(values);
      } else {
        this.destination.next(values.slice());
      }
    }
  }

  private _tryResultSelector(values: any[]) {
    let result: any;
    try {
      result = this.resultSelector.apply(this, values);
    } catch (err) {
      this.destination.error(err);
      return;
    }
    this.destination.next(result);
  }
}
