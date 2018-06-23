import { Observable } from '../Observable';
export declare function scan<T>(this: Observable<T>, accumulator: (acc: T, value: T, index: number) => T, seed?: T): Observable<T>;
export declare function scan<T>(this: Observable<T>, accumulator: (acc: T[], value: T, index: number) => T[], seed?: T[]): Observable<T[]>;
export declare function scan<T, R>(this: Observable<T>, accumulator: (acc: R, value: T, index: number) => R, seed?: R): Observable<R>;
