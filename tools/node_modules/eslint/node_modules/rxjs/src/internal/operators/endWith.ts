import { Observable } from '../Observable';
import { fromArray } from '../observable/fromArray';
import { scalar } from '../observable/scalar';
import { empty } from '../observable/empty';
import { concat as concatStatic } from '../observable/concat';
import { isScheduler } from '../util/isScheduler';
import { MonoTypeOperatorFunction, SchedulerLike, OperatorFunction } from '../types';

/* tslint:disable:max-line-length */
export function endWith<T>(scheduler?: SchedulerLike): MonoTypeOperatorFunction<T>;
export function endWith<T, A = T>(v1: A, scheduler?: SchedulerLike): OperatorFunction<T, T | A>;
export function endWith<T, A = T, B = T>(v1: A, v2: B, scheduler?: SchedulerLike): OperatorFunction<T, T | A | B>;
export function endWith<T, A = T, B = T, C = T>(v1: A, v2: B, v3: C, scheduler?: SchedulerLike): OperatorFunction<T, T | A | B | C>;
export function endWith<T, A = T, B = T, C = T, D = T>(v1: A, v2: B, v3: C, v4: D, scheduler?: SchedulerLike): OperatorFunction<T, T | A | B | C | D>;
export function endWith<T, A = T, B = T, C = T, D = T, E = T>(v1: A, v2: B, v3: C, v4: D, v5: E, scheduler?: SchedulerLike): OperatorFunction<T, T | A | B | C | D | E>;
export function endWith<T, A = T, B = T, C = T, D = T, E = T, F = T>(v1: A, v2: B, v3: C, v4: D, v5: E, v6: F, scheduler?: SchedulerLike): OperatorFunction<T, T | A | B | C | D | E | F>;
export function endWith<T, Z = T>(...array: Array<Z | SchedulerLike>): OperatorFunction<T, T | Z>;
/* tslint:enable:max-line-length */

/**
 * Returns an Observable that emits the items you specify as arguments after it finishes emitting
 * items emitted by the source Observable.
 *
 * ![](endWith.png)
 *
 * ## Example
 * ### After the source observable completes, appends an emission and then completes too.
 *
 * ```javascript
 * import { of } from 'rxjs';
 * import { endWith } from 'rxjs/operators';
 *
 * of('hi', 'how are you?', 'sorry, I have to go now').pipe(
 *   endWith('goodbye!'),
 * )
 * .subscribe(word => console.log(word));
 * // result:
 * // 'hi'
 * // 'how are you?'
 * // 'sorry, I have to go now'
 * // 'goodbye!'
 * ```
 *
 * @param {...T} values - Items you want the modified Observable to emit last.
 * @param {SchedulerLike} [scheduler] - A {@link SchedulerLike} to use for scheduling
 * the emissions of the `next` notifications.
 * @return {Observable} An Observable that emits the items emitted by the source Observable
 *  and then emits the items in the specified Iterable.
 * @method endWith
 * @owner Observable
 */
export function endWith<T>(...array: Array<T | SchedulerLike>): MonoTypeOperatorFunction<T> {
  return (source: Observable<T>) => {
    let scheduler = <SchedulerLike>array[array.length - 1];
    if (isScheduler(scheduler)) {
      array.pop();
    } else {
      scheduler = null;
    }

    const len = array.length;
    if (len === 1 && !scheduler) {
      return concatStatic(source, scalar(array[0] as T));
    } else if (len > 0) {
      return concatStatic(source, fromArray(array as T[], scheduler));
    } else {
      return concatStatic(source, empty(scheduler));
    }
  };
}
