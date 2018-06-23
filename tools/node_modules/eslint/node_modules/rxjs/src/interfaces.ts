import { Observable } from './Observable';

export type UnaryFunction<T, R> = (source: T) => R;

export type OperatorFunction<T, R> = UnaryFunction<Observable<T>, Observable<R>>;

export type FactoryOrValue<T> = T | (() => T);

export type MonoTypeOperatorFunction<T> = OperatorFunction<T, T>;
