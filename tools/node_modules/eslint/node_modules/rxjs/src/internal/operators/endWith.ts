import { Observable } from '../Observable';
import { fromArray } from '../observable/fromArray';
import { scalar } from '../observable/scalar';
import { empty } from '../observable/empty';
import { concat as concatStatic } from '../observable/concat';
import { isScheduler } from '../util/isScheduler';
import { MonoTypeOperatorFunction, SchedulerLike } from '../types';

/* tslint:disable:max-line-length */
export function endWith<T>(scheduler?: SchedulerLike): MonoTypeOperatorFunction<T>;
export function endWith<T>(v1: T, scheduler?: SchedulerLike): MonoTypeOperatorFunction<T>;
export function endWith<T>(v1: T, v2: T, scheduler?: SchedulerLike): MonoTypeOperatorFunction<T>;
export function endWith<T>(v1: T, v2: T, v3: T, scheduler?: SchedulerLike): MonoTypeOperatorFunction<T>;
export function endWith<T>(v1: T, v2: T, v3: T, v4: T, scheduler?: SchedulerLike): MonoTypeOperatorFunction<T>;
export function endWith<T>(v1: T, v2: T, v3: T, v4: T, v5: T, scheduler?: SchedulerLike): MonoTypeOperatorFunction<T>;
export function endWith<T>(v1: T, v2: T, v3: T, v4: T, v5: T, v6: T, scheduler?: SchedulerLike): MonoTypeOperatorFunction<T>;
export function endWith<T>(...array: Array<T | SchedulerLike>): MonoTypeOperatorFunction<T>;
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
      return concatStatic<T>(source, empty(scheduler) as any);
    }
  };
}
