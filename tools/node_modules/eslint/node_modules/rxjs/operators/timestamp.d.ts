import { IScheduler } from '../Scheduler';
import { OperatorFunction } from '../interfaces';
/**
 * @param scheduler
 * @return {Observable<Timestamp<any>>|WebSocketSubject<T>|Observable<T>}
 * @method timestamp
 * @owner Observable
 */
export declare function timestamp<T>(scheduler?: IScheduler): OperatorFunction<T, Timestamp<T>>;
export declare class Timestamp<T> {
    value: T;
    timestamp: number;
    constructor(value: T, timestamp: number);
}
