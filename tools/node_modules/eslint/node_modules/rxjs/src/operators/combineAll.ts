import { CombineLatestOperator } from '../operators/combineLatest';
import { Observable } from '../Observable';
import { OperatorFunction } from '../interfaces';

export function combineAll<T, R>(project?: (...values: Array<any>) => R): OperatorFunction<T, R> {
  return (source: Observable<T>) => source.lift(new CombineLatestOperator(project));
}
