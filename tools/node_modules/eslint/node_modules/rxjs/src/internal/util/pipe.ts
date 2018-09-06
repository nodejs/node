import { noop } from './noop';
import { UnaryFunction } from '../types';

/* tslint:disable:max-line-length */
export function pipe<T>(): UnaryFunction<T, T>;
export function pipe<T, A>(op1: UnaryFunction<T, A>): UnaryFunction<T, A>;
export function pipe<T, A, B>(op1: UnaryFunction<T, A>, op2: UnaryFunction<A, B>): UnaryFunction<T, B>;
export function pipe<T, A, B, C>(op1: UnaryFunction<T, A>, op2: UnaryFunction<A, B>, op3: UnaryFunction<B, C>): UnaryFunction<T, C>;
export function pipe<T, A, B, C, D>(op1: UnaryFunction<T, A>, op2: UnaryFunction<A, B>, op3: UnaryFunction<B, C>, op4: UnaryFunction<C, D>): UnaryFunction<T, D>;
export function pipe<T, A, B, C, D, E>(op1: UnaryFunction<T, A>, op2: UnaryFunction<A, B>, op3: UnaryFunction<B, C>, op4: UnaryFunction<C, D>, op5: UnaryFunction<D, E>): UnaryFunction<T, E>;
export function pipe<T, A, B, C, D, E, F>(op1: UnaryFunction<T, A>, op2: UnaryFunction<A, B>, op3: UnaryFunction<B, C>, op4: UnaryFunction<C, D>, op5: UnaryFunction<D, E>, op6: UnaryFunction<E, F>): UnaryFunction<T, F>;
export function pipe<T, A, B, C, D, E, F, G>(op1: UnaryFunction<T, A>, op2: UnaryFunction<A, B>, op3: UnaryFunction<B, C>, op4: UnaryFunction<C, D>, op5: UnaryFunction<D, E>, op6: UnaryFunction<E, F>, op7: UnaryFunction<F, G>): UnaryFunction<T, G>;
export function pipe<T, A, B, C, D, E, F, G, H>(op1: UnaryFunction<T, A>, op2: UnaryFunction<A, B>, op3: UnaryFunction<B, C>, op4: UnaryFunction<C, D>, op5: UnaryFunction<D, E>, op6: UnaryFunction<E, F>, op7: UnaryFunction<F, G>, op8: UnaryFunction<G, H>): UnaryFunction<T, H>;
export function pipe<T, A, B, C, D, E, F, G, H, I>(op1: UnaryFunction<T, A>, op2: UnaryFunction<A, B>, op3: UnaryFunction<B, C>, op4: UnaryFunction<C, D>, op5: UnaryFunction<D, E>, op6: UnaryFunction<E, F>, op7: UnaryFunction<F, G>, op8: UnaryFunction<G, H>, op9: UnaryFunction<H, I>): UnaryFunction<T, I>;
/* tslint:enable:max-line-length */

export function pipe<T, R>(...fns: Array<UnaryFunction<T, R>>): UnaryFunction<T, R> {
  return pipeFromArray(fns);
}

/** @internal */
export function pipeFromArray<T, R>(fns: Array<UnaryFunction<T, R>>): UnaryFunction<T, R> {
  if (!fns) {
    return noop as UnaryFunction<any, any>;
  }

  if (fns.length === 1) {
    return fns[0];
  }

  return function piped(input: T): R {
    return fns.reduce((prev: any, fn: UnaryFunction<T, R>) => fn(prev), input as any);
  };
}
