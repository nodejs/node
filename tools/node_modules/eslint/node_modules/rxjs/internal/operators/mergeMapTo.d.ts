import { OperatorFunction } from '../../internal/types';
import { ObservableInput } from '../types';
export declare function mergeMapTo<T>(innerObservable: ObservableInput<T>, concurrent?: number): OperatorFunction<any, T>;
/** @deprecated */
export declare function mergeMapTo<T, I, R>(innerObservable: ObservableInput<I>, resultSelector: (outerValue: T, innerValue: I, outerIndex: number, innerIndex: number) => R, concurrent?: number): OperatorFunction<T, R>;
