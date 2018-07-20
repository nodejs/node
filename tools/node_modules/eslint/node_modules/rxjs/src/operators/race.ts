import { Observable } from '../Observable';
import { isArray } from '../util/isArray';
import { MonoTypeOperatorFunction, OperatorFunction } from '../interfaces';
import { race as raceStatic } from '../observable/race';

/* tslint:disable:max-line-length */
export function race<T>(observables: Array<Observable<T>>): MonoTypeOperatorFunction<T>;
export function race<T, R>(observables: Array<Observable<T>>): OperatorFunction<T, R>;
export function race<T>(...observables: Array<Observable<T> | Array<Observable<T>>>): MonoTypeOperatorFunction<T>;
export function race<T, R>(...observables: Array<Observable<any> | Array<Observable<any>>>): OperatorFunction<T, R>;
/* tslint:enable:max-line-length */

/**
 * Returns an Observable that mirrors the first source Observable to emit an item
 * from the combination of this Observable and supplied Observables.
 * @param {...Observables} ...observables Sources used to race for which Observable emits first.
 * @return {Observable} An Observable that mirrors the output of the first Observable to emit an item.
 * @method race
 * @owner Observable
 */
export function race<T>(...observables: Array<Observable<T> | Array<Observable<T>>>): MonoTypeOperatorFunction<T> {
  return function raceOperatorFunction(source: Observable<T>) {
    // if the only argument is an array, it was most likely called with
    // `pair([obs1, obs2, ...])`
    if (observables.length === 1 && isArray(observables[0])) {
      observables = <Array<Observable<T>>>observables[0];
    }

    return source.lift.call(raceStatic<T>(source, ...observables));
  };
}