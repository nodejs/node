import { Observable, ObservableInput } from '../Observable';
export declare function switchMap<T, R>(this: Observable<T>, project: (value: T, index: number) => ObservableInput<R>): Observable<R>;
export declare function switchMap<T, I, R>(this: Observable<T>, project: (value: T, index: number) => ObservableInput<I>, resultSelector: (outerValue: T, innerValue: I, outerIndex: number, innerIndex: number) => R): Observable<R>;
