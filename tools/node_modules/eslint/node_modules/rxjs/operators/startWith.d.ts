import { IScheduler } from '../Scheduler';
import { MonoTypeOperatorFunction } from '../interfaces';
export declare function startWith<T>(v1: T, scheduler?: IScheduler): MonoTypeOperatorFunction<T>;
export declare function startWith<T>(v1: T, v2: T, scheduler?: IScheduler): MonoTypeOperatorFunction<T>;
export declare function startWith<T>(v1: T, v2: T, v3: T, scheduler?: IScheduler): MonoTypeOperatorFunction<T>;
export declare function startWith<T>(v1: T, v2: T, v3: T, v4: T, scheduler?: IScheduler): MonoTypeOperatorFunction<T>;
export declare function startWith<T>(v1: T, v2: T, v3: T, v4: T, v5: T, scheduler?: IScheduler): MonoTypeOperatorFunction<T>;
export declare function startWith<T>(v1: T, v2: T, v3: T, v4: T, v5: T, v6: T, scheduler?: IScheduler): MonoTypeOperatorFunction<T>;
export declare function startWith<T>(...array: Array<T | IScheduler>): MonoTypeOperatorFunction<T>;
