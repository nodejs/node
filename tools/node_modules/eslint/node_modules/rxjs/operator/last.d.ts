import { Observable } from '../Observable';
export declare function last<T, S extends T>(this: Observable<T>, predicate: (value: T, index: number, source: Observable<T>) => value is S): Observable<S>;
export declare function last<T, S extends T, R>(this: Observable<T>, predicate: (value: T | S, index: number, source: Observable<T>) => value is S, resultSelector: (value: S, index: number) => R, defaultValue?: R): Observable<R>;
export declare function last<T, S extends T>(this: Observable<T>, predicate: (value: T, index: number, source: Observable<T>) => value is S, resultSelector: void, defaultValue?: S): Observable<S>;
export declare function last<T>(this: Observable<T>, predicate?: (value: T, index: number, source: Observable<T>) => boolean): Observable<T>;
export declare function last<T, R>(this: Observable<T>, predicate: (value: T, index: number, source: Observable<T>) => boolean, resultSelector?: (value: T, index: number) => R, defaultValue?: R): Observable<R>;
export declare function last<T>(this: Observable<T>, predicate: (value: T, index: number, source: Observable<T>) => boolean, resultSelector: void, defaultValue?: T): Observable<T>;
