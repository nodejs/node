import { ObservableInput } from '../Observable';
import { OperatorFunction } from '../interfaces';
export declare function switchMapTo<T, R>(observable: ObservableInput<R>): OperatorFunction<T, R>;
export declare function switchMapTo<T, I, R>(observable: ObservableInput<I>, resultSelector: (outerValue: T, innerValue: I, outerIndex: number, innerIndex: number) => R): OperatorFunction<T, R>;
