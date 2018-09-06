import { ObservableInput, OperatorFunction } from '../types';
export declare function exhaustMap<T, R>(project: (value: T, index: number) => ObservableInput<R>): OperatorFunction<T, R>;
/** @deprecated resultSelector is no longer supported. Use inner map instead. */
export declare function exhaustMap<T, R>(project: (value: T, index: number) => ObservableInput<R>, resultSelector: undefined): OperatorFunction<T, R>;
/** @deprecated resultSelector is no longer supported. Use inner map instead. */
export declare function exhaustMap<T, I, R>(project: (value: T, index: number) => ObservableInput<I>, resultSelector: (outerValue: T, innerValue: I, outerIndex: number, innerIndex: number) => R): OperatorFunction<T, R>;
