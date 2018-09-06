import { MonoTypeOperatorFunction, OperatorFunction, SchedulerLike } from '../types';
export declare function startWith<T>(scheduler?: SchedulerLike): MonoTypeOperatorFunction<T>;
export declare function startWith<T, D = T>(v1: D, scheduler?: SchedulerLike): OperatorFunction<T, T | D>;
export declare function startWith<T, D = T, E = T>(v1: D, v2: E, scheduler?: SchedulerLike): OperatorFunction<T, T | D | E>;
export declare function startWith<T, D = T, E = T, F = T>(v1: D, v2: E, v3: F, scheduler?: SchedulerLike): OperatorFunction<T, T | D | E | F>;
export declare function startWith<T, D = T, E = T, F = T, G = T>(v1: D, v2: E, v3: F, v4: G, scheduler?: SchedulerLike): OperatorFunction<T, T | D | E | F | G>;
export declare function startWith<T, D = T, E = T, F = T, G = T, H = T>(v1: D, v2: E, v3: F, v4: G, v5: H, scheduler?: SchedulerLike): OperatorFunction<T, T | D | E | F | G | H>;
export declare function startWith<T, D = T, E = T, F = T, G = T, H = T, I = T>(v1: D, v2: E, v3: F, v4: G, v5: H, v6: I, scheduler?: SchedulerLike): OperatorFunction<T, T | D | E | F | G | H | I>;
export declare function startWith<T, D = T>(...array: Array<D | SchedulerLike>): OperatorFunction<T, T | D>;
