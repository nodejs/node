import { OperatorFunction, MonoTypeOperatorFunction } from '../types';
export declare function takeWhile<T, S extends T>(predicate: (value: T, index: number) => value is S): OperatorFunction<T, S>;
export declare function takeWhile<T>(predicate: (value: T, index: number) => boolean): MonoTypeOperatorFunction<T>;
