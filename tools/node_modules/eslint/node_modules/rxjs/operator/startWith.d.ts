import { IScheduler } from '../Scheduler';
import { Observable } from '../Observable';
export declare function startWith<T>(this: Observable<T>, v1: T, scheduler?: IScheduler): Observable<T>;
export declare function startWith<T>(this: Observable<T>, v1: T, v2: T, scheduler?: IScheduler): Observable<T>;
export declare function startWith<T>(this: Observable<T>, v1: T, v2: T, v3: T, scheduler?: IScheduler): Observable<T>;
export declare function startWith<T>(this: Observable<T>, v1: T, v2: T, v3: T, v4: T, scheduler?: IScheduler): Observable<T>;
export declare function startWith<T>(this: Observable<T>, v1: T, v2: T, v3: T, v4: T, v5: T, scheduler?: IScheduler): Observable<T>;
export declare function startWith<T>(this: Observable<T>, v1: T, v2: T, v3: T, v4: T, v5: T, v6: T, scheduler?: IScheduler): Observable<T>;
export declare function startWith<T>(this: Observable<T>, ...array: Array<T | IScheduler>): Observable<T>;
