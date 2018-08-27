import { Observable, ObservableInput } from '../Observable';
export declare function exhaustMap<T, R>(this: Observable<T>, project: (value: T, index: number) => ObservableInput<R>): Observable<R>;
export declare function exhaustMap<T, I, R>(this: Observable<T>, project: (value: T, index: number) => ObservableInput<I>, resultSelector: (outerValue: T, innerValue: I, outerIndex: number, innerIndex: number) => R): Observable<R>;
