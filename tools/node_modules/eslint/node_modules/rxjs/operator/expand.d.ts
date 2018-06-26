import { Observable } from '../Observable';
import { IScheduler } from '../Scheduler';
export declare function expand<T>(this: Observable<T>, project: (value: T, index: number) => Observable<T>, concurrent?: number, scheduler?: IScheduler): Observable<T>;
export declare function expand<T, R>(this: Observable<T>, project: (value: T, index: number) => Observable<R>, concurrent?: number, scheduler?: IScheduler): Observable<R>;
