import { IScheduler } from '../Scheduler';
import { ObservableInput } from '../Observable';
import { OperatorFunction, MonoTypeOperatorFunction } from '../interfaces';
export declare function timeoutWith<T>(due: number | Date, withObservable: ObservableInput<T>, scheduler?: IScheduler): MonoTypeOperatorFunction<T>;
export declare function timeoutWith<T, R>(due: number | Date, withObservable: ObservableInput<R>, scheduler?: IScheduler): OperatorFunction<T, T | R>;
