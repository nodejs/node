import { ZipOperator } from './zip';
import { Observable } from '../Observable';
import { OperatorFunction } from '../interfaces';

export function zipAll<T, R>(project?: (...values: Array<any>) => R): OperatorFunction<T, R> {
  return (source: Observable<T>) => source.lift(new ZipOperator(project));
}
