import { IScheduler } from '../Scheduler';
import { Observable } from '../Observable';
import { Subscriber } from '../Subscriber';
import { TeardownLogic } from '../Subscription';
/**
 * We need this JSDoc comment for affecting ESDoc.
 * @extends {Ignored}
 * @hide true
 */
export declare class ArrayObservable<T> extends Observable<T> {
    private array;
    private scheduler;
    static create<T>(array: T[], scheduler?: IScheduler): Observable<T>;
    static of<T>(item1: T, scheduler?: IScheduler): Observable<T>;
    static of<T>(item1: T, item2: T, scheduler?: IScheduler): Observable<T>;
    static of<T>(item1: T, item2: T, item3: T, scheduler?: IScheduler): Observable<T>;
    static of<T>(item1: T, item2: T, item3: T, item4: T, scheduler?: IScheduler): Observable<T>;
    static of<T>(item1: T, item2: T, item3: T, item4: T, item5: T, scheduler?: IScheduler): Observable<T>;
    static of<T>(item1: T, item2: T, item3: T, item4: T, item5: T, item6: T, scheduler?: IScheduler): Observable<T>;
    static of<T>(...array: Array<T | IScheduler>): Observable<T>;
    static dispatch(state: any): void;
    value: any;
    constructor(array: T[], scheduler?: IScheduler);
    /** @deprecated internal use only */ _subscribe(subscriber: Subscriber<T>): TeardownLogic;
}
