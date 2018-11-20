import { Observable } from '../Observable';
import { OperatorFunction } from '../../internal/types';
import { mergeMap } from './mergeMap';
import { ObservableInput } from '../types';

/* tslint:disable:max-line-length */
export function mergeMapTo<T>(innerObservable: ObservableInput<T>, concurrent?: number): OperatorFunction<any, T>;
/** @deprecated */
export function mergeMapTo<T, I, R>(innerObservable: ObservableInput<I>, resultSelector: (outerValue: T, innerValue: I, outerIndex: number, innerIndex: number) => R, concurrent?: number): OperatorFunction<T, R>;
/* tslint:enable:max-line-length */

/**
 * Projects each source value to the same Observable which is merged multiple
 * times in the output Observable.
 *
 * <span class="informal">It's like {@link mergeMap}, but maps each value always
 * to the same inner Observable.</span>
 *
 * ![](mergeMapTo.png)
 *
 * Maps each source value to the given Observable `innerObservable` regardless
 * of the source value, and then merges those resulting Observables into one
 * single Observable, which is the output Observable.
 *
 * ## Example
 * For each click event, start an interval Observable ticking every 1 second
 * ```javascript
 * const clicks = fromEvent(document, 'click');
 * const result = clicks.pipe(mergeMapTo(interval(1000)));
 * result.subscribe(x => console.log(x));
 * ```
 *
 * @see {@link concatMapTo}
 * @see {@link merge}
 * @see {@link mergeAll}
 * @see {@link mergeMap}
 * @see {@link mergeScan}
 * @see {@link switchMapTo}
 *
 * @param {ObservableInput} innerObservable An Observable to replace each value from
 * the source Observable.
 * @param {number} [concurrent=Number.POSITIVE_INFINITY] Maximum number of input
 * Observables being subscribed to concurrently.
 * @return {Observable} An Observable that emits items from the given
 * `innerObservable`
 * @method mergeMapTo
 * @owner Observable
 */
export function mergeMapTo<T, I, R>(
  innerObservable: ObservableInput<I>,
  resultSelector?: ((outerValue: T, innerValue: I, outerIndex: number, innerIndex: number) => R) | number,
  concurrent: number = Number.POSITIVE_INFINITY
): OperatorFunction<T, I|R> {
  if (typeof resultSelector === 'function') {
    return mergeMap(() => innerObservable, resultSelector, concurrent);
  }
  if (typeof resultSelector === 'number') {
    concurrent = resultSelector;
  }
  return mergeMap(() => innerObservable, concurrent);
}
