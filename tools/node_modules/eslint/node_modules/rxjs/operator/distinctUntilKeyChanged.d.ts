import { Observable } from '../Observable';
export declare function distinctUntilKeyChanged<T>(this: Observable<T>, key: string): Observable<T>;
export declare function distinctUntilKeyChanged<T, K>(this: Observable<T>, key: string, compare: (x: K, y: K) => boolean): Observable<T>;
