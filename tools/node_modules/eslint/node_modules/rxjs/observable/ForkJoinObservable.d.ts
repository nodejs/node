import { Observable, SubscribableOrPromise } from '../Observable';
import { Subscriber } from '../Subscriber';
import { Subscription } from '../Subscription';
/**
 * We need this JSDoc comment for affecting ESDoc.
 * @extends {Ignored}
 * @hide true
 */
export declare class ForkJoinObservable<T> extends Observable<T> {
    private sources;
    private resultSelector;
    constructor(sources: Array<SubscribableOrPromise<any>>, resultSelector?: (...values: Array<any>) => T);
    static create<T, T2>(v1: SubscribableOrPromise<T>, v2: SubscribableOrPromise<T2>): Observable<[T, T2]>;
    static create<T, T2, T3>(v1: SubscribableOrPromise<T>, v2: SubscribableOrPromise<T2>, v3: SubscribableOrPromise<T3>): Observable<[T, T2, T3]>;
    static create<T, T2, T3, T4>(v1: SubscribableOrPromise<T>, v2: SubscribableOrPromise<T2>, v3: SubscribableOrPromise<T3>, v4: SubscribableOrPromise<T4>): Observable<[T, T2, T3, T4]>;
    static create<T, T2, T3, T4, T5>(v1: SubscribableOrPromise<T>, v2: SubscribableOrPromise<T2>, v3: SubscribableOrPromise<T3>, v4: SubscribableOrPromise<T4>, v5: SubscribableOrPromise<T5>): Observable<[T, T2, T3, T4, T5]>;
    static create<T, T2, T3, T4, T5, T6>(v1: SubscribableOrPromise<T>, v2: SubscribableOrPromise<T2>, v3: SubscribableOrPromise<T3>, v4: SubscribableOrPromise<T4>, v5: SubscribableOrPromise<T5>, v6: SubscribableOrPromise<T6>): Observable<[T, T2, T3, T4, T5, T6]>;
    static create<T, R>(v1: SubscribableOrPromise<T>, project: (v1: T) => R): Observable<R>;
    static create<T, T2, R>(v1: SubscribableOrPromise<T>, v2: SubscribableOrPromise<T2>, project: (v1: T, v2: T2) => R): Observable<R>;
    static create<T, T2, T3, R>(v1: SubscribableOrPromise<T>, v2: SubscribableOrPromise<T2>, v3: SubscribableOrPromise<T3>, project: (v1: T, v2: T2, v3: T3) => R): Observable<R>;
    static create<T, T2, T3, T4, R>(v1: SubscribableOrPromise<T>, v2: SubscribableOrPromise<T2>, v3: SubscribableOrPromise<T3>, v4: SubscribableOrPromise<T4>, project: (v1: T, v2: T2, v3: T3, v4: T4) => R): Observable<R>;
    static create<T, T2, T3, T4, T5, R>(v1: SubscribableOrPromise<T>, v2: SubscribableOrPromise<T2>, v3: SubscribableOrPromise<T3>, v4: SubscribableOrPromise<T4>, v5: SubscribableOrPromise<T5>, project: (v1: T, v2: T2, v3: T3, v4: T4, v5: T5) => R): Observable<R>;
    static create<T, T2, T3, T4, T5, T6, R>(v1: SubscribableOrPromise<T>, v2: SubscribableOrPromise<T2>, v3: SubscribableOrPromise<T3>, v4: SubscribableOrPromise<T4>, v5: SubscribableOrPromise<T5>, v6: SubscribableOrPromise<T6>, project: (v1: T, v2: T2, v3: T3, v4: T4, v5: T5, v6: T6) => R): Observable<R>;
    static create<T>(sources: SubscribableOrPromise<T>[]): Observable<T[]>;
    static create<R>(sources: SubscribableOrPromise<any>[]): Observable<R>;
    static create<T, R>(sources: SubscribableOrPromise<T>[], project: (...values: Array<T>) => R): Observable<R>;
    static create<R>(sources: SubscribableOrPromise<any>[], project: (...values: Array<any>) => R): Observable<R>;
    static create<T>(...sources: SubscribableOrPromise<T>[]): Observable<T[]>;
    static create<R>(...sources: SubscribableOrPromise<any>[]): Observable<R>;
    /** @deprecated internal use only */ _subscribe(subscriber: Subscriber<any>): Subscription;
}
