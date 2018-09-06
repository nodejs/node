import { Observable } from '../Observable';
import { ObservableInput, SchedulerLike } from '../types';
export declare function from<T>(input: ObservableInput<T>, scheduler?: SchedulerLike): Observable<T>;
export declare function from<T>(input: ObservableInput<ObservableInput<T>>, scheduler?: SchedulerLike): Observable<Observable<T>>;
