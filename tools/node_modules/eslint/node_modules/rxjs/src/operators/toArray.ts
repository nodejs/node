import { reduce } from './reduce';
import { OperatorFunction } from '../interfaces';

function toArrayReducer<T>(arr: T[], item: T, index: number) {
  if (index === 0) {
    return [item];
  }
  arr.push(item);
  return arr;
}

export function toArray<T>(): OperatorFunction<T, T[]> {
  return reduce(toArrayReducer, []) as OperatorFunction<T, T[]>;
}
