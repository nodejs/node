import { MonoTypeOperatorFunction, OperatorFunction, SchedulerLike } from '../types';
export declare function publishReplay<T>(bufferSize?: number, windowTime?: number, scheduler?: SchedulerLike): MonoTypeOperatorFunction<T>;
export declare function publishReplay<T, R>(bufferSize?: number, windowTime?: number, selector?: OperatorFunction<T, R>, scheduler?: SchedulerLike): OperatorFunction<T, R>;
export declare function publishReplay<T>(bufferSize?: number, windowTime?: number, selector?: MonoTypeOperatorFunction<T>, scheduler?: SchedulerLike): MonoTypeOperatorFunction<T>;
