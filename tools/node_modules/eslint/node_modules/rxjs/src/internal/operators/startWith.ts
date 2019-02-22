import { Observable } from '../Observable';
import { fromArray } from '../observable/fromArray';
import { scalar } from '../observable/scalar';
import { empty } from '../observable/empty';
import { concat as concatStatic } from '../observable/concat';
import { isScheduler } from '../util/isScheduler';
import { MonoTypeOperatorFunction, OperatorFunction, SchedulerLike } from '../types';

/* tslint:disable:max-line-length */
export function startWith<T>(scheduler?: SchedulerLike): MonoTypeOperatorFunction<T>;
export function startWith<T, D = T>(v1: D, scheduler?: SchedulerLike): OperatorFunction<T, T | D>;
export function startWith<T, D = T, E = T>(v1: D, v2: E, scheduler?: SchedulerLike): OperatorFunction<T, T | D | E>;
export function startWith<T, D = T, E = T, F = T>(v1: D, v2: E, v3: F, scheduler?: SchedulerLike): OperatorFunction<T, T | D | E | F>;
export function startWith<T, D = T, E = T, F = T, G = T>(v1: D, v2:  E, v3: F, v4: G, scheduler?: SchedulerLike): OperatorFunction<T, T | D | E | F | G>;
export function startWith<T, D = T, E = T, F = T, G = T, H = T>(v1: D, v2: E, v3: F, v4: G, v5: H, scheduler?: SchedulerLike): OperatorFunction<T, T | D | E | F | G | H>;
export function startWith<T, D = T, E = T, F = T, G = T, H = T, I = T>(v1: D, v2: E, v3: F, v4: G, v5: H, v6: I, scheduler?: SchedulerLike): OperatorFunction<T, T | D | E | F | G | H | I>;
export function startWith<T, D = T>(...array: Array<D | SchedulerLike>): OperatorFunction<T, T | D>;
/* tslint:enable:max-line-length */

/**
 * Returns an Observable that emits the items you specify as arguments before it begins to emit
 * items emitted by the source Observable.
 *
 * <span class="informal">First emits its arguments in order, and then any
 * emissions from the source.</span>
 *
 * ![](startWith.png)
 *
 * ## Examples
 *
 * Start the chain of emissions with `"first"`, `"second"`
 *
 * ```javascript
 * import { of } from 'rxjs';
 * import { startWith } from 'rxjs/operators';
 *
 * of("from source")
 *   .pipe(startWith("first", "second"))
 *   .subscribe(x => console.log(x));
 *
 * // results:
 * //   "first"
 * //   "second"
 * //   "from source"
 * ```
 *
 * @param {...T} values - Items you want the modified Observable to emit first.
 * @param {SchedulerLike} [scheduler] - A {@link SchedulerLike} to use for scheduling
 * the emissions of the `next` notifications.
 * @return {Observable} An Observable that emits the items in the specified Iterable and then emits the items
 * emitted by the source Observable.
 * @method startWith
 * @owner Observable
 */
export function startWith<T, D>(...array: Array<T | SchedulerLike>): OperatorFunction<T, T | D> {
  return (source: Observable<T>) => {
    let scheduler = <SchedulerLike>array[array.length - 1];
    if (isScheduler(scheduler)) {
      array.pop();
    } else {
      scheduler = null;
    }

    const len = array.length;
    if (len === 1 && !scheduler) {
      return concatStatic(scalar(array[0] as T), source);
    } else if (len > 0) {
      return concatStatic(fromArray(array as T[], scheduler), source);
    } else {
      return concatStatic(empty(scheduler), source);
    }
  };
}
