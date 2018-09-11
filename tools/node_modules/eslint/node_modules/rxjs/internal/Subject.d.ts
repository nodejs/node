import { Operator } from './Operator';
import { Observable } from './Observable';
import { Subscriber } from './Subscriber';
import { Subscription } from './Subscription';
import { Observer, SubscriptionLike, TeardownLogic } from './types';
/**
 * @class SubjectSubscriber<T>
 */
export declare class SubjectSubscriber<T> extends Subscriber<T> {
    protected destination: Subject<T>;
    constructor(destination: Subject<T>);
}
/**
 * @class Subject<T>
 */
export declare class Subject<T> extends Observable<T> implements SubscriptionLike {
    observers: Observer<T>[];
    closed: boolean;
    isStopped: boolean;
    hasError: boolean;
    thrownError: any;
    constructor();
    /**@nocollapse */
    static create: Function;
    lift<R>(operator: Operator<T, R>): Observable<R>;
    next(value?: T): void;
    error(err: any): void;
    complete(): void;
    unsubscribe(): void;
    /** @deprecated This is an internal implementation detail, do not use. */
    _trySubscribe(subscriber: Subscriber<T>): TeardownLogic;
    /** @deprecated This is an internal implementation detail, do not use. */
    _subscribe(subscriber: Subscriber<T>): Subscription;
    asObservable(): Observable<T>;
}
/**
 * @class AnonymousSubject<T>
 */
export declare class AnonymousSubject<T> extends Subject<T> {
    protected destination?: Observer<T>;
    constructor(destination?: Observer<T>, source?: Observable<T>);
    next(value: T): void;
    error(err: any): void;
    complete(): void;
    /** @deprecated This is an internal implementation detail, do not use. */
    _subscribe(subscriber: Subscriber<T>): Subscription;
}
