import { Observable } from './Observable';
export declare type UnaryFunction<T, R> = (source: T) => R;
export declare type OperatorFunction<T, R> = UnaryFunction<Observable<T>, Observable<R>>;
export declare type FactoryOrValue<T> = T | (() => T);
export declare type MonoTypeOperatorFunction<T> = OperatorFunction<T, T>;
