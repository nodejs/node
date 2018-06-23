import { IScheduler } from '../Scheduler';
import { Observable } from '../Observable';
export declare function bufferTime<T>(this: Observable<T>, bufferTimeSpan: number, scheduler?: IScheduler): Observable<T[]>;
export declare function bufferTime<T>(this: Observable<T>, bufferTimeSpan: number, bufferCreationInterval: number, scheduler?: IScheduler): Observable<T[]>;
export declare function bufferTime<T>(this: Observable<T>, bufferTimeSpan: number, bufferCreationInterval: number, maxBufferSize: number, scheduler?: IScheduler): Observable<T[]>;
