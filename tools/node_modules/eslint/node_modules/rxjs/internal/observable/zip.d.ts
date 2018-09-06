import { Observable } from '../Observable';
import { Operator } from '../Operator';
import { ObservableInput } from '../types';
import { Subscriber } from '../Subscriber';
/** @deprecated resultSelector is no longer supported, pipe to map instead */
export declare function zip<T, R>(v1: ObservableInput<T>, resultSelector: (v1: T) => R): Observable<R>;
/** @deprecated resultSelector is no longer supported, pipe to map instead */
export declare function zip<T, T2, R>(v1: ObservableInput<T>, v2: ObservableInput<T2>, resultSelector: (v1: T, v2: T2) => R): Observable<R>;
/** @deprecated resultSelector is no longer supported, pipe to map instead */
export declare function zip<T, T2, T3, R>(v1: ObservableInput<T>, v2: ObservableInput<T2>, v3: ObservableInput<T3>, resultSelector: (v1: T, v2: T2, v3: T3) => R): Observable<R>;
/** @deprecated resultSelector is no longer supported, pipe to map instead */
export declare function zip<T, T2, T3, T4, R>(v1: ObservableInput<T>, v2: ObservableInput<T2>, v3: ObservableInput<T3>, v4: ObservableInput<T4>, resultSelector: (v1: T, v2: T2, v3: T3, v4: T4) => R): Observable<R>;
/** @deprecated resultSelector is no longer supported, pipe to map instead */
export declare function zip<T, T2, T3, T4, T5, R>(v1: ObservableInput<T>, v2: ObservableInput<T2>, v3: ObservableInput<T3>, v4: ObservableInput<T4>, v5: ObservableInput<T5>, resultSelector: (v1: T, v2: T2, v3: T3, v4: T4, v5: T5) => R): Observable<R>;
/** @deprecated resultSelector is no longer supported, pipe to map instead */
export declare function zip<T, T2, T3, T4, T5, T6, R>(v1: ObservableInput<T>, v2: ObservableInput<T2>, v3: ObservableInput<T3>, v4: ObservableInput<T4>, v5: ObservableInput<T5>, v6: ObservableInput<T6>, resultSelector: (v1: T, v2: T2, v3: T3, v4: T4, v5: T5, v6: T6) => R): Observable<R>;
export declare function zip<T, T2>(v1: ObservableInput<T>, v2: ObservableInput<T2>): Observable<[T, T2]>;
export declare function zip<T, T2, T3>(v1: ObservableInput<T>, v2: ObservableInput<T2>, v3: ObservableInput<T3>): Observable<[T, T2, T3]>;
export declare function zip<T, T2, T3, T4>(v1: ObservableInput<T>, v2: ObservableInput<T2>, v3: ObservableInput<T3>, v4: ObservableInput<T4>): Observable<[T, T2, T3, T4]>;
export declare function zip<T, T2, T3, T4, T5>(v1: ObservableInput<T>, v2: ObservableInput<T2>, v3: ObservableInput<T3>, v4: ObservableInput<T4>, v5: ObservableInput<T5>): Observable<[T, T2, T3, T4, T5]>;
export declare function zip<T, T2, T3, T4, T5, T6>(v1: ObservableInput<T>, v2: ObservableInput<T2>, v3: ObservableInput<T3>, v4: ObservableInput<T4>, v5: ObservableInput<T5>, v6: ObservableInput<T6>): Observable<[T, T2, T3, T4, T5, T6]>;
export declare function zip<T>(array: ObservableInput<T>[]): Observable<T[]>;
export declare function zip<R>(array: ObservableInput<any>[]): Observable<R>;
/** @deprecated resultSelector is no longer supported, pipe to map instead */
export declare function zip<T, R>(array: ObservableInput<T>[], resultSelector: (...values: Array<T>) => R): Observable<R>;
/** @deprecated resultSelector is no longer supported, pipe to map instead */
export declare function zip<R>(array: ObservableInput<any>[], resultSelector: (...values: Array<any>) => R): Observable<R>;
export declare function zip<T>(...observables: Array<ObservableInput<T>>): Observable<T[]>;
export declare function zip<T, R>(...observables: Array<ObservableInput<T> | ((...values: Array<T>) => R)>): Observable<R>;
export declare function zip<R>(...observables: Array<ObservableInput<any> | ((...values: Array<any>) => R)>): Observable<R>;
export declare class ZipOperator<T, R> implements Operator<T, R> {
    resultSelector: (...values: Array<any>) => R;
    constructor(resultSelector?: (...values: Array<any>) => R);
    call(subscriber: Subscriber<R>, source: any): any;
}
/**
 * We need this JSDoc comment for affecting ESDoc.
 * @ignore
 * @extends {Ignored}
 */
export declare class ZipSubscriber<T, R> extends Subscriber<T> {
    private values;
    private resultSelector;
    private iterators;
    private active;
    constructor(destination: Subscriber<R>, resultSelector?: (...values: Array<any>) => R, values?: any);
    protected _next(value: any): void;
    protected _complete(): void;
    notifyInactive(): void;
    checkIterators(): void;
    protected _tryresultSelector(args: any[]): void;
}
