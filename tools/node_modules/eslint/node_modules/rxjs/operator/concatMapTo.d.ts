import { Observable, ObservableInput } from '../Observable';
export declare function concatMapTo<T, R>(this: Observable<T>, observable: ObservableInput<R>): Observable<R>;
export declare function concatMapTo<T, I, R>(this: Observable<T>, observable: ObservableInput<I>, resultSelector: (outerValue: T, innerValue: I, outerIndex: number, innerIndex: number) => R): Observable<R>;
