import { OperatorFunction, MonoTypeOperatorFunction } from '../interfaces';
export declare function defaultIfEmpty<T>(defaultValue?: T): MonoTypeOperatorFunction<T>;
export declare function defaultIfEmpty<T, R>(defaultValue?: R): OperatorFunction<T, T | R>;
