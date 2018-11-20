import { SchedulerLike, OperatorFunction } from '../types';
export declare function timeInterval<T>(scheduler?: SchedulerLike): OperatorFunction<T, TimeInterval<T>>;
export declare class TimeInterval<T> {
    value: T;
    interval: number;
    constructor(value: T, interval: number);
}
