import { MonoTypeOperatorFunction, SchedulerLike, OperatorFunction } from '../types';
export declare function endWith<T>(scheduler?: SchedulerLike): MonoTypeOperatorFunction<T>;
export declare function endWith<T, A = T>(v1: A, scheduler?: SchedulerLike): OperatorFunction<T, T | A>;
export declare function endWith<T, A = T, B = T>(v1: A, v2: B, scheduler?: SchedulerLike): OperatorFunction<T, T | A | B>;
export declare function endWith<T, A = T, B = T, C = T>(v1: A, v2: B, v3: C, scheduler?: SchedulerLike): OperatorFunction<T, T | A | B | C>;
export declare function endWith<T, A = T, B = T, C = T, D = T>(v1: A, v2: B, v3: C, v4: D, scheduler?: SchedulerLike): OperatorFunction<T, T | A | B | C | D>;
export declare function endWith<T, A = T, B = T, C = T, D = T, E = T>(v1: A, v2: B, v3: C, v4: D, v5: E, scheduler?: SchedulerLike): OperatorFunction<T, T | A | B | C | D | E>;
export declare function endWith<T, A = T, B = T, C = T, D = T, E = T, F = T>(v1: A, v2: B, v3: C, v4: D, v5: E, v6: F, scheduler?: SchedulerLike): OperatorFunction<T, T | A | B | C | D | E | F>;
export declare function endWith<T, Z = T>(...array: Array<Z | SchedulerLike>): OperatorFunction<T, T | Z>;
