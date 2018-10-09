import { MonoTypeOperatorFunction, SchedulerLike } from '../types';
export declare function endWith<T>(scheduler?: SchedulerLike): MonoTypeOperatorFunction<T>;
export declare function endWith<T>(v1: T, scheduler?: SchedulerLike): MonoTypeOperatorFunction<T>;
export declare function endWith<T>(v1: T, v2: T, scheduler?: SchedulerLike): MonoTypeOperatorFunction<T>;
export declare function endWith<T>(v1: T, v2: T, v3: T, scheduler?: SchedulerLike): MonoTypeOperatorFunction<T>;
export declare function endWith<T>(v1: T, v2: T, v3: T, v4: T, scheduler?: SchedulerLike): MonoTypeOperatorFunction<T>;
export declare function endWith<T>(v1: T, v2: T, v3: T, v4: T, v5: T, scheduler?: SchedulerLike): MonoTypeOperatorFunction<T>;
export declare function endWith<T>(v1: T, v2: T, v3: T, v4: T, v5: T, v6: T, scheduler?: SchedulerLike): MonoTypeOperatorFunction<T>;
export declare function endWith<T>(...array: Array<T | SchedulerLike>): MonoTypeOperatorFunction<T>;
