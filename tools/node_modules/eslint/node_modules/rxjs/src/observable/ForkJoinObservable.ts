import { Observable, SubscribableOrPromise } from '../Observable';
import { Subscriber } from '../Subscriber';
import { Subscription } from '../Subscription';
import { EmptyObservable } from './EmptyObservable';
import { isArray } from '../util/isArray';

import { subscribeToResult } from '../util/subscribeToResult';
import { OuterSubscriber } from '../OuterSubscriber';
import { InnerSubscriber } from '../InnerSubscriber';

/**
 * We need this JSDoc comment for affecting ESDoc.
 * @extends {Ignored}
 * @hide true
 */
export class ForkJoinObservable<T> extends Observable<T> {
  constructor(private sources: Array<SubscribableOrPromise<any>>,
              private resultSelector?: (...values: Array<any>) => T) {
    super();
  }

  /* tslint:disable:max-line-length */
  static create<T, T2>(v1: SubscribableOrPromise<T>, v2: SubscribableOrPromise<T2>): Observable<[T, T2]>;
  static create<T, T2, T3>(v1: SubscribableOrPromise<T>, v2: SubscribableOrPromise<T2>, v3: SubscribableOrPromise<T3>): Observable<[T, T2, T3]>;
  static create<T, T2, T3, T4>(v1: SubscribableOrPromise<T>, v2: SubscribableOrPromise<T2>, v3: SubscribableOrPromise<T3>, v4: SubscribableOrPromise<T4>): Observable<[T, T2, T3, T4]>;
  static create<T, T2, T3, T4, T5>(v1: SubscribableOrPromise<T>, v2: SubscribableOrPromise<T2>, v3: SubscribableOrPromise<T3>, v4: SubscribableOrPromise<T4>, v5: SubscribableOrPromise<T5>): Observable<[T, T2, T3, T4, T5]>;
  static create<T, T2, T3, T4, T5, T6>(v1: SubscribableOrPromise<T>, v2: SubscribableOrPromise<T2>, v3: SubscribableOrPromise<T3>, v4: SubscribableOrPromise<T4>, v5: SubscribableOrPromise<T5>, v6: SubscribableOrPromise<T6>): Observable<[T, T2, T3, T4, T5, T6]>;
  static create<T, R>(v1: SubscribableOrPromise<T>, project: (v1: T) => R): Observable<R>;
  static create<T, T2, R>(v1: SubscribableOrPromise<T>, v2: SubscribableOrPromise<T2>, project: (v1: T, v2: T2) => R): Observable<R>;
  static create<T, T2, T3, R>(v1: SubscribableOrPromise<T>, v2: SubscribableOrPromise<T2>, v3: SubscribableOrPromise<T3>, project: (v1: T, v2: T2, v3: T3) => R): Observable<R>;
  static create<T, T2, T3, T4, R>(v1: SubscribableOrPromise<T>, v2: SubscribableOrPromise<T2>, v3: SubscribableOrPromise<T3>, v4: SubscribableOrPromise<T4>, project: (v1: T, v2: T2, v3: T3, v4: T4) => R): Observable<R>;
  static create<T, T2, T3, T4, T5, R>(v1: SubscribableOrPromise<T>, v2: SubscribableOrPromise<T2>, v3: SubscribableOrPromise<T3>, v4: SubscribableOrPromise<T4>, v5: SubscribableOrPromise<T5>, project: (v1: T, v2: T2, v3: T3, v4: T4, v5: T5) => R): Observable<R>;
  static create<T, T2, T3, T4, T5, T6, R>(v1: SubscribableOrPromise<T>, v2: SubscribableOrPromise<T2>, v3: SubscribableOrPromise<T3>, v4: SubscribableOrPromise<T4>, v5: SubscribableOrPromise<T5>, v6: SubscribableOrPromise<T6>, project: (v1: T, v2: T2, v3: T3, v4: T4, v5: T5, v6: T6) => R): Observable<R>;
  static create<T>(sources: SubscribableOrPromise<T>[]): Observable<T[]>;
  static create<R>(sources: SubscribableOrPromise<any>[]): Observable<R>;
  static create<T, R>(sources: SubscribableOrPromise<T>[], project: (...values: Array<T>) => R): Observable<R>;
  static create<R>(sources: SubscribableOrPromise<any>[], project: (...values: Array<any>) => R): Observable<R>;
  static create<T>(...sources: SubscribableOrPromise<T>[]): Observable<T[]>;
  static create<R>(...sources: SubscribableOrPromise<any>[]): Observable<R>;
  /* tslint:enable:max-line-length */

  /**
   * Joins last values emitted by passed Observables.
   *
   * <span class="informal">Wait for Observables to complete and then combine last values they emitted.</span>
   *
   * <img src="./img/forkJoin.png" width="100%">
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
   * @example <caption>Use forkJoin with operator emitting immediately</caption>
   * const observable = Rx.Observable.forkJoin(
   *   Rx.Observable.of(1, 2, 3, 4),
   *   Rx.Observable.of(5, 6, 7, 8)
   * );
   * observable.subscribe(
   *   value => console.log(value),
   *   err => {},
   *   () => console.log('This is how it ends!')
   * );
   *
   * // Logs:
   * // [4, 8]
   * // "This is how it ends!"
   *
   *
   * @example <caption>Use forkJoin with operator emitting after some time</caption>
   * const observable = Rx.Observable.forkJoin(
   *   Rx.Observable.interval(1000).take(3), // emit 0, 1, 2 every second and complete
   *   Rx.Observable.interval(500).take(4) // emit 0, 1, 2, 3 every half a second and complete
   * );
   * observable.subscribe(
   *   value => console.log(value),
   *   err => {},
   *   () => console.log('This is how it ends!')
   * );
   *
   * // Logs:
   * // [2, 3] after 3 seconds
   * // "This is how it ends!" immediately after
   *
   *
   * @example <caption>Use forkJoin with project function</caption>
   * const observable = Rx.Observable.forkJoin(
   *   Rx.Observable.interval(1000).take(3), // emit 0, 1, 2 every second and complete
   *   Rx.Observable.interval(500).take(4), // emit 0, 1, 2, 3 every half a second and complete
   *   (n, m) => n + m
   * );
   * observable.subscribe(
   *   value => console.log(value),
   *   err => {},
   *   () => console.log('This is how it ends!')
   * );
   *
   * // Logs:
   * // 5 after 3 seconds
   * // "This is how it ends!" immediately after
   *
   * @see {@link combineLatest}
   * @see {@link zip}
   *
   * @param {...SubscribableOrPromise} sources Any number of Observables provided either as an array or as an arguments
   * passed directly to the operator.
   * @param {function} [project] Function that takes values emitted by input Observables and returns value
   * that will appear in resulting Observable instead of default array.
   * @return {Observable} Observable emitting either an array of last values emitted by passed Observables
   * or value from project function.
   * @static true
   * @name forkJoin
   * @owner Observable
   */
  static create<T>(...sources: Array<SubscribableOrPromise<any> |
                                  Array<SubscribableOrPromise<any>> |
                                  ((...values: Array<any>) => any)>): Observable<T> {
    if (sources === null || arguments.length === 0) {
      return new EmptyObservable<T>();
    }

    let resultSelector: (...values: Array<any>) => any = null;
    if (typeof sources[sources.length - 1] === 'function') {
      resultSelector = <(...values: Array<any>) => any>sources.pop();
    }

    // if the first and only other argument besides the resultSelector is an array
    // assume it's been called with `forkJoin([obs1, obs2, obs3], resultSelector)`
    if (sources.length === 1 && isArray(sources[0])) {
      sources = <Array<SubscribableOrPromise<any>>>sources[0];
    }

    if (sources.length === 0) {
      return new EmptyObservable<T>();
    }

    return new ForkJoinObservable(<Array<SubscribableOrPromise<any>>>sources, resultSelector);
  }

  /** @deprecated internal use only */ _subscribe(subscriber: Subscriber<any>): Subscription {
    return new ForkJoinSubscriber(subscriber, this.sources, this.resultSelector);
  }
}

/**
 * We need this JSDoc comment for affecting ESDoc.
 * @ignore
 * @extends {Ignored}
 */
class ForkJoinSubscriber<T> extends OuterSubscriber<T, T> {
  private completed = 0;
  private total: number;
  private values: any[];
  private haveValues = 0;

  constructor(destination: Subscriber<T>,
              private sources: Array<SubscribableOrPromise<any>>,
              private resultSelector?: (...values: Array<any>) => T) {
    super(destination);

    const len = sources.length;
    this.total = len;
    this.values = new Array(len);

    for (let i = 0; i < len; i++) {
      const source = sources[i];
      const innerSubscription = subscribeToResult(this, source, null, i);

      if (innerSubscription) {
        (<any> innerSubscription).outerIndex = i;
        this.add(innerSubscription);
      }
    }
  }

  notifyNext(outerValue: any, innerValue: T,
             outerIndex: number, innerIndex: number,
             innerSub: InnerSubscriber<T, T>): void {
    this.values[outerIndex] = innerValue;
    if (!(<any>innerSub)._hasValue) {
      (<any>innerSub)._hasValue = true;
      this.haveValues++;
    }
  }

  notifyComplete(innerSub: InnerSubscriber<T, T>): void {
    const destination = this.destination;
    const { haveValues, resultSelector, values } = this;
    const len = values.length;

    if (!(<any>innerSub)._hasValue) {
      destination.complete();
      return;
    }

    this.completed++;

    if (this.completed !== len) {
      return;
    }

    if (haveValues === len) {
      const value = resultSelector ? resultSelector.apply(this, values) : values;
      destination.next(value);
    }

    destination.complete();
  }
}
