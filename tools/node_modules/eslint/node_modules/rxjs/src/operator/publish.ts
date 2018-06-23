
import { Observable } from '../Observable';
import { ConnectableObservable } from '../observable/ConnectableObservable';
import { publish as higherOrder } from '../operators/publish';

/* tslint:disable:max-line-length */
export function publish<T>(this: Observable<T>): ConnectableObservable<T>;
export function publish<T>(this: Observable<T>, selector: (source: Observable<T>) => Observable<T>): Observable<T>;
export function publish<T, R>(this: Observable<T>, selector: (source: Observable<T>) => Observable<R>): Observable<R>;
/* tslint:enable:max-line-length */

/**
 * Returns a ConnectableObservable, which is a variety of Observable that waits until its connect method is called
 * before it begins emitting items to those Observers that have subscribed to it.
 *
 * <img src="./img/publish.png" width="100%">
 *
 * @param {Function} [selector] - Optional selector function which can use the multicasted source sequence as many times
 * as needed, without causing multiple subscriptions to the source sequence.
 * Subscribers to the given source will receive all notifications of the source from the time of the subscription on.
 * @return A ConnectableObservable that upon connection causes the source Observable to emit items to its Observers.
 * @method publish
 * @owner Observable
 */
export function publish<T, R>(this: Observable<T>, selector?: (source: Observable<T>) => Observable<R>): Observable<R> | ConnectableObservable<R> {
  return higherOrder(selector)(this);
}

export type selector<T> = (source: Observable<T>) => Observable<T>;
