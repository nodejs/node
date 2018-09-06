import { ObservableInput, OperatorFunction } from '../types';
export declare function concatMapTo<T>(observable: ObservableInput<T>): OperatorFunction<any, T>;
/** @deprecated */
export declare function concatMapTo<T>(observable: ObservableInput<T>, resultSelector: undefined): OperatorFunction<any, T>;
/** @deprecated */
export declare function concatMapTo<T, I, R>(observable: ObservableInput<I>, resultSelector: (outerValue: T, innerValue: I, outerIndex: number, innerIndex: number) => R): OperatorFunction<T, R>;
