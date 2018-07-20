import { Observable } from '../Observable';
import { zipAll as higherOrder } from '../operators/zipAll';

/**
 * @param project
 * @return {Observable<R>|WebSocketSubject<T>|Observable<T>}
 * @method zipAll
 * @owner Observable
 */
export function zipAll<T, R>(this: Observable<T>, project?: (...values: Array<any>) => R): Observable<R> {
  return higherOrder(project)(this);
}
