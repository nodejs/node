import { Observable } from '../Observable';
import { ObservableInput } from '../types';
export declare function forkJoin<T>(sources: [ObservableInput<T>]): Observable<T[]>;
export declare function forkJoin<T, T2>(sources: [ObservableInput<T>, ObservableInput<T2>]): Observable<[T, T2]>;
export declare function forkJoin<T, T2, T3>(sources: [ObservableInput<T>, ObservableInput<T2>, ObservableInput<T3>]): Observable<[T, T2, T3]>;
export declare function forkJoin<T, T2, T3, T4>(sources: [ObservableInput<T>, ObservableInput<T2>, ObservableInput<T3>, ObservableInput<T4>]): Observable<[T, T2, T3, T4]>;
export declare function forkJoin<T, T2, T3, T4, T5>(sources: [ObservableInput<T>, ObservableInput<T2>, ObservableInput<T3>, ObservableInput<T4>, ObservableInput<T5>]): Observable<[T, T2, T3, T4, T5]>;
export declare function forkJoin<T, T2, T3, T4, T5, T6>(sources: [ObservableInput<T>, ObservableInput<T2>, ObservableInput<T3>, ObservableInput<T4>, ObservableInput<T5>, ObservableInput<T6>]): Observable<[T, T2, T3, T4, T5, T6]>;
export declare function forkJoin<T>(sources: Array<ObservableInput<T>>): Observable<T[]>;
export declare function forkJoin<T>(v1: ObservableInput<T>): Observable<T[]>;
export declare function forkJoin<T, T2>(v1: ObservableInput<T>, v2: ObservableInput<T2>): Observable<[T, T2]>;
export declare function forkJoin<T, T2, T3>(v1: ObservableInput<T>, v2: ObservableInput<T2>, v3: ObservableInput<T3>): Observable<[T, T2, T3]>;
export declare function forkJoin<T, T2, T3, T4>(v1: ObservableInput<T>, v2: ObservableInput<T2>, v3: ObservableInput<T3>, v4: ObservableInput<T4>): Observable<[T, T2, T3, T4]>;
export declare function forkJoin<T, T2, T3, T4, T5>(v1: ObservableInput<T>, v2: ObservableInput<T2>, v3: ObservableInput<T3>, v4: ObservableInput<T4>, v5: ObservableInput<T5>): Observable<[T, T2, T3, T4, T5]>;
export declare function forkJoin<T, T2, T3, T4, T5, T6>(v1: ObservableInput<T>, v2: ObservableInput<T2>, v3: ObservableInput<T3>, v4: ObservableInput<T4>, v5: ObservableInput<T5>, v6: ObservableInput<T6>): Observable<[T, T2, T3, T4, T5, T6]>;
/** @deprecated resultSelector is deprecated, pipe to map instead */
export declare function forkJoin(...args: Array<ObservableInput<any> | Function>): Observable<any>;
export declare function forkJoin<T>(...sources: ObservableInput<T>[]): Observable<T[]>;
