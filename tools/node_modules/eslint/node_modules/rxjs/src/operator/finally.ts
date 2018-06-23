
import { Observable } from '../Observable';
import { finalize } from '../operators/finalize';

/**
 * Returns an Observable that mirrors the source Observable, but will call a specified function when
 * the source terminates on complete or error.
 * @param {function} callback Function to be called when source terminates.
 * @return {Observable} An Observable that mirrors the source, but will call the specified function on termination.
 * @method finally
 * @owner Observable
 */
export function _finally<T>(this: Observable<T>, callback: () => void): Observable<T> {
  return finalize(callback)(this) as Observable<T>;
}
