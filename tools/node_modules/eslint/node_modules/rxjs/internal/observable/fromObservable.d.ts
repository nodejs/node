import { Observable } from '../Observable';
import { InteropObservable, SchedulerLike } from '../types';
export declare function fromObservable<T>(input: InteropObservable<T>, scheduler: SchedulerLike): Observable<T>;
