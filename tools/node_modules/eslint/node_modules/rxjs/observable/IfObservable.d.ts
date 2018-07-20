import { Observable, SubscribableOrPromise } from '../Observable';
import { Subscriber } from '../Subscriber';
import { TeardownLogic } from '../Subscription';
/**
 * We need this JSDoc comment for affecting ESDoc.
 * @extends {Ignored}
 * @hide true
 */
export declare class IfObservable<T, R> extends Observable<T> {
    private condition;
    private thenSource;
    private elseSource;
    static create<T, R>(condition: () => boolean | void, thenSource?: SubscribableOrPromise<T> | void, elseSource?: SubscribableOrPromise<R> | void): Observable<T | R>;
    constructor(condition: () => boolean | void, thenSource?: SubscribableOrPromise<T> | void, elseSource?: SubscribableOrPromise<R> | void);
    /** @deprecated internal use only */ _subscribe(subscriber: Subscriber<T | R>): TeardownLogic;
}
