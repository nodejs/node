import { Observable } from '../Observable';
export declare function defaultIfEmpty<T>(this: Observable<T>, defaultValue?: T): Observable<T>;
export declare function defaultIfEmpty<T, R>(this: Observable<T>, defaultValue?: R): Observable<T | R>;
