import { IScheduler } from '../Scheduler';
import { Observable, ObservableInput } from '../Observable';
export declare function timeoutWith<T>(this: Observable<T>, due: number | Date, withObservable: ObservableInput<T>, scheduler?: IScheduler): Observable<T>;
export declare function timeoutWith<T, R>(this: Observable<T>, due: number | Date, withObservable: ObservableInput<R>, scheduler?: IScheduler): Observable<T | R>;
