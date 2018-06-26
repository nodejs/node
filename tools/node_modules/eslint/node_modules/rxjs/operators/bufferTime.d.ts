import { IScheduler } from '../Scheduler';
import { OperatorFunction } from '../interfaces';
export declare function bufferTime<T>(bufferTimeSpan: number, scheduler?: IScheduler): OperatorFunction<T, T[]>;
export declare function bufferTime<T>(bufferTimeSpan: number, bufferCreationInterval: number, scheduler?: IScheduler): OperatorFunction<T, T[]>;
export declare function bufferTime<T>(bufferTimeSpan: number, bufferCreationInterval: number, maxBufferSize: number, scheduler?: IScheduler): OperatorFunction<T, T[]>;
