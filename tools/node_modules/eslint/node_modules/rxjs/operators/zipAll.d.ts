import { OperatorFunction } from '../interfaces';
export declare function zipAll<T, R>(project?: (...values: Array<any>) => R): OperatorFunction<T, R>;
