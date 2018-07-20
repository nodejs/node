import { Observable } from '../Observable';
import { OperatorFunction, MonoTypeOperatorFunction } from '../interfaces';
export declare function first<T, S extends T>(predicate: (value: T, index: number, source: Observable<T>) => value is S): OperatorFunction<T, S>;
export declare function first<T, S extends T, R>(predicate: (value: T | S, index: number, source: Observable<T>) => value is S, resultSelector: (value: S, index: number) => R, defaultValue?: R): OperatorFunction<T, R>;
export declare function first<T, S extends T>(predicate: (value: T, index: number, source: Observable<T>) => value is S, resultSelector: void, defaultValue?: S): OperatorFunction<T, S>;
export declare function first<T>(predicate?: (value: T, index: number, source: Observable<T>) => boolean): MonoTypeOperatorFunction<T>;
export declare function first<T, R>(predicate: (value: T, index: number, source: Observable<T>) => boolean, resultSelector?: (value: T, index: number) => R, defaultValue?: R): OperatorFunction<T, R>;
export declare function first<T>(predicate: (value: T, index: number, source: Observable<T>) => boolean, resultSelector: void, defaultValue?: T): MonoTypeOperatorFunction<T>;
