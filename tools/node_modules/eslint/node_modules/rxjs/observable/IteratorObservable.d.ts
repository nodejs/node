import { IScheduler } from '../Scheduler';
import { Observable } from '../Observable';
import { TeardownLogic } from '../Subscription';
import { Subscriber } from '../Subscriber';
/**
 * We need this JSDoc comment for affecting ESDoc.
 * @extends {Ignored}
 * @hide true
 */
export declare class IteratorObservable<T> extends Observable<T> {
    private scheduler;
    private iterator;
    static create<T>(iterator: any, scheduler?: IScheduler): IteratorObservable<T>;
    static dispatch(state: any): void;
    constructor(iterator: any, scheduler?: IScheduler);
    /** @deprecated internal use only */ _subscribe(subscriber: Subscriber<T>): TeardownLogic;
}
