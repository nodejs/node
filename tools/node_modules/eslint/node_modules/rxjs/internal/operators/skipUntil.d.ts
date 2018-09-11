import { Observable } from '../Observable';
import { MonoTypeOperatorFunction } from '../types';
/**
 * Returns an Observable that skips items emitted by the source Observable until a second Observable emits an item.
 *
 * ![](skipUntil.png)
 *
 * @param {Observable} notifier - The second Observable that has to emit an item before the source Observable's elements begin to
 * be mirrored by the resulting Observable.
 * @return {Observable<T>} An Observable that skips items from the source Observable until the second Observable emits
 * an item, then emits the remaining items.
 * @method skipUntil
 * @owner Observable
 */
export declare function skipUntil<T>(notifier: Observable<any>): MonoTypeOperatorFunction<T>;
