import { OperatorFunction, MonoTypeOperatorFunction } from '../interfaces';
export declare function scan<T>(accumulator: (acc: T, value: T, index: number) => T, seed?: T): MonoTypeOperatorFunction<T>;
export declare function scan<T>(accumulator: (acc: T[], value: T, index: number) => T[], seed?: T[]): OperatorFunction<T, T[]>;
export declare function scan<T, R>(accumulator: (acc: R, value: T, index: number) => R, seed?: R): OperatorFunction<T, R>;
