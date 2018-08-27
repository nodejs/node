import { IScheduler } from '../Scheduler';
import { Observable, ObservableInput } from '../Observable';
import { Subscriber } from '../Subscriber';
/**
 * We need this JSDoc comment for affecting ESDoc.
 * @extends {Ignored}
 * @hide true
 */
export declare class FromObservable<T> extends Observable<T> {
    private ish;
    private scheduler;
    constructor(ish: ObservableInput<T>, scheduler?: IScheduler);
    static create<T>(ish: ObservableInput<T>, scheduler?: IScheduler): Observable<T>;
    static create<T, R>(ish: ArrayLike<T>, scheduler?: IScheduler): Observable<R>;
    /** @deprecated internal use only */ _subscribe(subscriber: Subscriber<T>): any;
}
