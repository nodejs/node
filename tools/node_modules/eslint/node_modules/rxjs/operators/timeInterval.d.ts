import { IScheduler } from '../Scheduler';
import { OperatorFunction } from '../interfaces';
export declare function timeInterval<T>(scheduler?: IScheduler): OperatorFunction<T, TimeInterval<T>>;
export declare class TimeInterval<T> {
    value: T;
    interval: number;
    constructor(value: T, interval: number);
}
