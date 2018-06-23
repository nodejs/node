import { Observable, ObservableInput } from '../Observable';
import { IScheduler } from '../Scheduler';
import { OperatorFunction, MonoTypeOperatorFunction } from '../interfaces';
import { merge as mergeStatic } from '../observable/merge';

export { merge as mergeStatic } from '../observable/merge';

/* tslint:disable:max-line-length */
export function merge<T>(scheduler?: IScheduler): MonoTypeOperatorFunction<T>;
export function merge<T>(concurrent?: number, scheduler?: IScheduler): MonoTypeOperatorFunction<T>;
export function merge<T, T2>(v2: ObservableInput<T2>, scheduler?: IScheduler): OperatorFunction<T, T | T2>;
export function merge<T, T2>(v2: ObservableInput<T2>, concurrent?: number, scheduler?: IScheduler): OperatorFunction<T, T | T2>;
export function merge<T, T2, T3>(v2: ObservableInput<T2>, v3: ObservableInput<T3>, scheduler?: IScheduler): OperatorFunction<T, T | T2 | T3>;
export function merge<T, T2, T3>(v2: ObservableInput<T2>, v3: ObservableInput<T3>, concurrent?: number, scheduler?: IScheduler): OperatorFunction<T, T | T2 | T3>;
export function merge<T, T2, T3, T4>(v2: ObservableInput<T2>, v3: ObservableInput<T3>, v4: ObservableInput<T4>, scheduler?: IScheduler): OperatorFunction<T, T | T2 | T3 | T4>;
export function merge<T, T2, T3, T4>(v2: ObservableInput<T2>, v3: ObservableInput<T3>, v4: ObservableInput<T4>, concurrent?: number, scheduler?: IScheduler): OperatorFunction<T, T | T2 | T3 | T4>;
export function merge<T, T2, T3, T4, T5>(v2: ObservableInput<T2>, v3: ObservableInput<T3>, v4: ObservableInput<T4>, v5: ObservableInput<T5>, scheduler?: IScheduler): OperatorFunction<T, T | T2 | T3 | T4 | T5>;
export function merge<T, T2, T3, T4, T5>(v2: ObservableInput<T2>, v3: ObservableInput<T3>, v4: ObservableInput<T4>, v5: ObservableInput<T5>, concurrent?: number, scheduler?: IScheduler): OperatorFunction<T, T | T2 | T3 | T4 | T5>;
export function merge<T, T2, T3, T4, T5, T6>(v2: ObservableInput<T2>, v3: ObservableInput<T3>, v4: ObservableInput<T4>, v5: ObservableInput<T5>, v6: ObservableInput<T6>, scheduler?: IScheduler): OperatorFunction<T, T | T2 | T3 | T4 | T5 | T6>;
export function merge<T, T2, T3, T4, T5, T6>(v2: ObservableInput<T2>, v3: ObservableInput<T3>, v4: ObservableInput<T4>, v5: ObservableInput<T5>, v6: ObservableInput<T6>, concurrent?: number, scheduler?: IScheduler): OperatorFunction<T, T | T2 | T3 | T4 | T5 | T6>;
export function merge<T>(...observables: Array<ObservableInput<T> | IScheduler | number>): MonoTypeOperatorFunction<T>;
export function merge<T, R>(...observables: Array<ObservableInput<any> | IScheduler | number>): OperatorFunction<T, R>;
/* tslint:enable:max-line-length */
/**
 * Creates an output Observable which concurrently emits all values from every
 * given input Observable.
 *
 * <span class="informal">Flattens multiple Observables together by blending
 * their values into one Observable.</span>
 *
 * <img src="./img/merge.png" width="100%">
 *
 * `merge` subscribes to each given input Observable (either the source or an
 * Observable given as argument), and simply forwards (without doing any
 * transformation) all the values from all the input Observables to the output
 * Observable. The output Observable only completes once all input Observables
 * have completed. Any error delivered by an input Observable will be immediately
 * emitted on the output Observable.
 *
 * @example <caption>Merge together two Observables: 1s interval and clicks</caption>
 * var clicks = Rx.Observable.fromEvent(document, 'click');
 * var timer = Rx.Observable.interval(1000);
 * var clicksOrTimer = clicks.merge(timer);
 * clicksOrTimer.subscribe(x => console.log(x));
 *
 * @example <caption>Merge together 3 Observables, but only 2 run concurrently</caption>
 * var timer1 = Rx.Observable.interval(1000).take(10);
 * var timer2 = Rx.Observable.interval(2000).take(6);
 * var timer3 = Rx.Observable.interval(500).take(10);
 * var concurrent = 2; // the argument
 * var merged = timer1.merge(timer2, timer3, concurrent);
 * merged.subscribe(x => console.log(x));
 *
 * @see {@link mergeAll}
 * @see {@link mergeMap}
 * @see {@link mergeMapTo}
 * @see {@link mergeScan}
 *
 * @param {ObservableInput} other An input Observable to merge with the source
 * Observable. More than one input Observables may be given as argument.
 * @param {number} [concurrent=Number.POSITIVE_INFINITY] Maximum number of input
 * Observables being subscribed to concurrently.
 * @param {Scheduler} [scheduler=null] The IScheduler to use for managing
 * concurrency of input Observables.
 * @return {Observable} An Observable that emits items that are the result of
 * every input Observable.
 * @method merge
 * @owner Observable
 */
export function merge<T, R>(...observables: Array<ObservableInput<any> | IScheduler | number>): OperatorFunction<T, R> {
  return (source: Observable<T>) => source.lift.call(mergeStatic(source, ...observables));
}
