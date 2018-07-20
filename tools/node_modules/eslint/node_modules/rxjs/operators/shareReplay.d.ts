import { IScheduler } from '../Scheduler';
import { MonoTypeOperatorFunction } from '../interfaces';
/**
 * @method shareReplay
 * @owner Observable
 */
export declare function shareReplay<T>(bufferSize?: number, windowTime?: number, scheduler?: IScheduler): MonoTypeOperatorFunction<T>;
