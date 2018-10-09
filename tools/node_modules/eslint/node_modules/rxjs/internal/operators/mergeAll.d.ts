import { OperatorFunction, ObservableInput } from '../types';
export declare function mergeAll<T>(concurrent?: number): OperatorFunction<ObservableInput<T>, T>;
