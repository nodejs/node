import { Observable } from '../Observable';
import { ObservableInput, SchedulerLike } from '../types';
import { Subscriber } from '../Subscriber';
import { OuterSubscriber } from '../OuterSubscriber';
import { Operator } from '../Operator';
import { InnerSubscriber } from '../InnerSubscriber';
/** @deprecated resultSelector no longer supported, pipe to map instead */
export declare function combineLatest<T, R>(v1: ObservableInput<T>, resultSelector: (v1: T) => R, scheduler?: SchedulerLike): Observable<R>;
/** @deprecated resultSelector no longer supported, pipe to map instead */
export declare function combineLatest<T, T2, R>(v1: ObservableInput<T>, v2: ObservableInput<T2>, resultSelector: (v1: T, v2: T2) => R, scheduler?: SchedulerLike): Observable<R>;
/** @deprecated resultSelector no longer supported, pipe to map instead */
export declare function combineLatest<T, T2, T3, R>(v1: ObservableInput<T>, v2: ObservableInput<T2>, v3: ObservableInput<T3>, resultSelector: (v1: T, v2: T2, v3: T3) => R, scheduler?: SchedulerLike): Observable<R>;
/** @deprecated resultSelector no longer supported, pipe to map instead */
export declare function combineLatest<T, T2, T3, T4, R>(v1: ObservableInput<T>, v2: ObservableInput<T2>, v3: ObservableInput<T3>, v4: ObservableInput<T4>, resultSelector: (v1: T, v2: T2, v3: T3, v4: T4) => R, scheduler?: SchedulerLike): Observable<R>;
/** @deprecated resultSelector no longer supported, pipe to map instead */
export declare function combineLatest<T, T2, T3, T4, T5, R>(v1: ObservableInput<T>, v2: ObservableInput<T2>, v3: ObservableInput<T3>, v4: ObservableInput<T4>, v5: ObservableInput<T5>, resultSelector: (v1: T, v2: T2, v3: T3, v4: T4, v5: T5) => R, scheduler?: SchedulerLike): Observable<R>;
/** @deprecated resultSelector no longer supported, pipe to map instead */
export declare function combineLatest<T, T2, T3, T4, T5, T6, R>(v1: ObservableInput<T>, v2: ObservableInput<T2>, v3: ObservableInput<T3>, v4: ObservableInput<T4>, v5: ObservableInput<T5>, v6: ObservableInput<T6>, resultSelector: (v1: T, v2: T2, v3: T3, v4: T4, v5: T5, v6: T6) => R, scheduler?: SchedulerLike): Observable<R>;
export declare function combineLatest<T, T2>(v1: ObservableInput<T>, v2: ObservableInput<T2>, scheduler?: SchedulerLike): Observable<[T, T2]>;
export declare function combineLatest<T, T2, T3>(v1: ObservableInput<T>, v2: ObservableInput<T2>, v3: ObservableInput<T3>, scheduler?: SchedulerLike): Observable<[T, T2, T3]>;
export declare function combineLatest<T, T2, T3, T4>(v1: ObservableInput<T>, v2: ObservableInput<T2>, v3: ObservableInput<T3>, v4: ObservableInput<T4>, scheduler?: SchedulerLike): Observable<[T, T2, T3, T4]>;
export declare function combineLatest<T, T2, T3, T4, T5>(v1: ObservableInput<T>, v2: ObservableInput<T2>, v3: ObservableInput<T3>, v4: ObservableInput<T4>, v5: ObservableInput<T5>, scheduler?: SchedulerLike): Observable<[T, T2, T3, T4, T5]>;
export declare function combineLatest<T, T2, T3, T4, T5, T6>(v1: ObservableInput<T>, v2: ObservableInput<T2>, v3: ObservableInput<T3>, v4: ObservableInput<T4>, v5: ObservableInput<T5>, v6: ObservableInput<T6>, scheduler?: SchedulerLike): Observable<[T, T2, T3, T4, T5, T6]>;
export declare function combineLatest<T>(array: ObservableInput<T>[], scheduler?: SchedulerLike): Observable<T[]>;
export declare function combineLatest<R>(array: ObservableInput<any>[], scheduler?: SchedulerLike): Observable<R>;
/** @deprecated resultSelector no longer supported, pipe to map instead */
export declare function combineLatest<T, R>(array: ObservableInput<T>[], resultSelector: (...values: Array<T>) => R, scheduler?: SchedulerLike): Observable<R>;
/** @deprecated resultSelector no longer supported, pipe to map instead */
export declare function combineLatest<R>(array: ObservableInput<any>[], resultSelector: (...values: Array<any>) => R, scheduler?: SchedulerLike): Observable<R>;
export declare function combineLatest<T>(...observables: Array<ObservableInput<T> | SchedulerLike>): Observable<T[]>;
export declare function combineLatest<T, R>(...observables: Array<ObservableInput<T> | ((...values: Array<T>) => R) | SchedulerLike>): Observable<R>;
export declare function combineLatest<R>(...observables: Array<ObservableInput<any> | ((...values: Array<any>) => R) | SchedulerLike>): Observable<R>;
export declare class CombineLatestOperator<T, R> implements Operator<T, R> {
    private resultSelector?;
    constructor(resultSelector?: (...values: Array<any>) => R);
    call(subscriber: Subscriber<R>, source: any): any;
}
/**
 * We need this JSDoc comment for affecting ESDoc.
 * @ignore
 * @extends {Ignored}
 */
export declare class CombineLatestSubscriber<T, R> extends OuterSubscriber<T, R> {
    private resultSelector?;
    private active;
    private values;
    private observables;
    private toRespond;
    constructor(destination: Subscriber<R>, resultSelector?: (...values: Array<any>) => R);
    protected _next(observable: any): void;
    protected _complete(): void;
    notifyComplete(unused: Subscriber<R>): void;
    notifyNext(outerValue: T, innerValue: R, outerIndex: number, innerIndex: number, innerSub: InnerSubscriber<T, R>): void;
    private _tryResultSelector;
}
