import { Observable, ObservableInput } from '../Observable';
export declare function mergeMapTo<T, R>(this: Observable<T>, observable: ObservableInput<R>, concurrent?: number): Observable<R>;
export declare function mergeMapTo<T, I, R>(this: Observable<T>, observable: ObservableInput<I>, resultSelector: (outerValue: T, innerValue: I, outerIndex: number, innerIndex: number) => R, concurrent?: number): Observable<R>;
