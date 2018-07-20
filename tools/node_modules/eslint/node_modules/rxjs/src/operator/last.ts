import { Observable } from '../Observable';
import { last as higherOrder } from '../operators/last';

/* tslint:disable:max-line-length */
export function last<T, S extends T>(this: Observable<T>,
                                     predicate: (value: T, index: number, source: Observable<T>) => value is S): Observable<S>;
export function last<T, S extends T, R>(this: Observable<T>,
                                        predicate: (value: T | S, index: number, source: Observable<T>) => value is S,
                                        resultSelector: (value: S, index: number) => R, defaultValue?: R): Observable<R>;
export function last<T, S extends T>(this: Observable<T>,
                                     predicate: (value: T, index: number, source: Observable<T>) => value is S,
                                     resultSelector: void,
                                     defaultValue?: S): Observable<S>;
export function last<T>(this: Observable<T>,
                        predicate?: (value: T, index: number, source: Observable<T>) => boolean): Observable<T>;
export function last<T, R>(this: Observable<T>,
                           predicate: (value: T, index: number, source: Observable<T>) => boolean,
                           resultSelector?: (value: T, index: number) => R,
                           defaultValue?: R): Observable<R>;
export function last<T>(this: Observable<T>,
                        predicate: (value: T, index: number, source: Observable<T>) => boolean,
                        resultSelector: void,
                        defaultValue?: T): Observable<T>;
/* tslint:enable:max-line-length */

/**
 * Returns an Observable that emits only the last item emitted by the source Observable.
 * It optionally takes a predicate function as a parameter, in which case, rather than emitting
 * the last item from the source Observable, the resulting Observable will emit the last item
 * from the source Observable that satisfies the predicate.
 *
 * <img src="./img/last.png" width="100%">
 *
 * @throws {EmptyError} Delivers an EmptyError to the Observer's `error`
 * callback if the Observable completes before any `next` notification was sent.
 * @param {function} predicate - The condition any source emitted item has to satisfy.
 * @return {Observable} An Observable that emits only the last item satisfying the given condition
 * from the source, or an NoSuchElementException if no such items are emitted.
 * @throws - Throws if no items that match the predicate are emitted by the source Observable.
 * @method last
 * @owner Observable
 */
export function last<T, R>(this: Observable<T>, predicate?: (value: T, index: number, source: Observable<T>) => boolean,
                           resultSelector?: ((value: T, index: number) => R) | void,
                           defaultValue?: R): Observable<T | R> {
  return higherOrder(predicate, resultSelector as any, defaultValue)(this);
}
