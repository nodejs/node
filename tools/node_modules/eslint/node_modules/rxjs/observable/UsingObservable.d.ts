import { Observable, SubscribableOrPromise } from '../Observable';
import { Subscriber } from '../Subscriber';
import { AnonymousSubscription, TeardownLogic } from '../Subscription';
/**
 * We need this JSDoc comment for affecting ESDoc.
 * @extends {Ignored}
 * @hide true
 */
export declare class UsingObservable<T> extends Observable<T> {
    private resourceFactory;
    private observableFactory;
    static create<T>(resourceFactory: () => AnonymousSubscription | void, observableFactory: (resource: AnonymousSubscription) => SubscribableOrPromise<T> | void): Observable<T>;
    constructor(resourceFactory: () => AnonymousSubscription | void, observableFactory: (resource: AnonymousSubscription) => SubscribableOrPromise<T> | void);
    /** @deprecated internal use only */ _subscribe(subscriber: Subscriber<T>): TeardownLogic;
}
