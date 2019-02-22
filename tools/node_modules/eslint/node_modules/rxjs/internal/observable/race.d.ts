import { Observable } from '../Observable';
import { Operator } from '../Operator';
import { Subscriber } from '../Subscriber';
import { TeardownLogic } from '../types';
import { OuterSubscriber } from '../OuterSubscriber';
import { InnerSubscriber } from '../InnerSubscriber';
export declare function race<A, B>(a: Observable<A>, b: Observable<B>): Observable<A> | Observable<B>;
export declare function race<A, B, C>(a: Observable<A>, b: Observable<B>, c: Observable<C>): Observable<A> | Observable<B> | Observable<C>;
export declare function race<A, B, C, D>(a: Observable<A>, b: Observable<B>, c: Observable<C>, d: Observable<D>): Observable<A> | Observable<B> | Observable<C> | Observable<D>;
export declare function race<A, B, C, D, E>(a: Observable<A>, b: Observable<B>, c: Observable<C>, d: Observable<D>, e: Observable<E>): Observable<A> | Observable<B> | Observable<C> | Observable<D> | Observable<E>;
export declare function race<T>(observables: Observable<T>[]): Observable<T>;
export declare function race(observables: Observable<any>[]): Observable<{}>;
export declare function race<T>(...observables: Observable<T>[]): Observable<T>;
export declare function race(...observables: Observable<any>[]): Observable<{}>;
export declare class RaceOperator<T> implements Operator<T, T> {
    call(subscriber: Subscriber<T>, source: any): TeardownLogic;
}
/**
 * We need this JSDoc comment for affecting ESDoc.
 * @ignore
 * @extends {Ignored}
 */
export declare class RaceSubscriber<T> extends OuterSubscriber<T, T> {
    private hasFirst;
    private observables;
    private subscriptions;
    constructor(destination: Subscriber<T>);
    protected _next(observable: any): void;
    protected _complete(): void;
    notifyNext(outerValue: T, innerValue: T, outerIndex: number, innerIndex: number, innerSub: InnerSubscriber<T, T>): void;
}
