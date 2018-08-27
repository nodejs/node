import { Observable } from '../Observable';

/**
 * @param func
 * @return {Observable<R>}
 * @method let
 * @owner Observable
 */
export function letProto<T, R>(this: Observable<T>, func: (selector: Observable<T>) => Observable<R>): Observable<R> {
  return func(this);
}
