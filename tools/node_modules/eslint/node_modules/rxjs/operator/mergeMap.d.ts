import { Observable, ObservableInput } from '../Observable';
export declare function mergeMap<T, R>(this: Observable<T>, project: (value: T, index: number) => ObservableInput<R>, concurrent?: number): Observable<R>;
export declare function mergeMap<T, I, R>(this: Observable<T>, project: (value: T, index: number) => ObservableInput<I>, resultSelector: (outerValue: T, innerValue: I, outerIndex: number, innerIndex: number) => R, concurrent?: number): Observable<R>;
