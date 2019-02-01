import { Observable } from '../Observable';
import { ObservableInput, SchedulerLike, ObservedValueOf } from '../types';
export declare function from<O extends ObservableInput<any>>(input: O, scheduler?: SchedulerLike): Observable<ObservedValueOf<O>>;
