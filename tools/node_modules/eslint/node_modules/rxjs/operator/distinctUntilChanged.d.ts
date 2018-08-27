import { Observable } from '../Observable';
export declare function distinctUntilChanged<T>(this: Observable<T>, compare?: (x: T, y: T) => boolean): Observable<T>;
export declare function distinctUntilChanged<T, K>(this: Observable<T>, compare: (x: K, y: K) => boolean, keySelector: (x: T) => K): Observable<T>;
