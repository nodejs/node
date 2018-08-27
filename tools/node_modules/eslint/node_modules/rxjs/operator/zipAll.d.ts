import { Observable } from '../Observable';
/**
 * @param project
 * @return {Observable<R>|WebSocketSubject<T>|Observable<T>}
 * @method zipAll
 * @owner Observable
 */
export declare function zipAll<T, R>(this: Observable<T>, project?: (...values: Array<any>) => R): Observable<R>;
