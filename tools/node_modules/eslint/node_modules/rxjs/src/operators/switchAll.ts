import { OperatorFunction } from '../interfaces';
import { Observable } from '../Observable';
import { switchMap } from './switchMap';
import { identity } from '../util/identity';

export function switchAll<T>(): OperatorFunction<Observable<T>, T> {
  return switchMap(identity);
}
