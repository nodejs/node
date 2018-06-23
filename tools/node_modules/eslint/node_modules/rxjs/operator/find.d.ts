import { Observable } from '../Observable';
export declare function find<T, S extends T>(this: Observable<T>, predicate: (value: T, index: number) => value is S, thisArg?: any): Observable<S>;
export declare function find<T>(this: Observable<T>, predicate: (value: T, index: number) => boolean, thisArg?: any): Observable<T>;
