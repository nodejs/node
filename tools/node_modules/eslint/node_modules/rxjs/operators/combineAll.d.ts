import { OperatorFunction } from '../interfaces';
export declare function combineAll<T, R>(project?: (...values: Array<any>) => R): OperatorFunction<T, R>;
