import { Observable } from '../Observable';
import { AsyncSubject } from '../AsyncSubject';
import { multicast } from './multicast';
import { ConnectableObservable } from '../observable/ConnectableObservable';
import { UnaryFunction } from '../interfaces';

export function publishLast<T>(): UnaryFunction<Observable<T>, ConnectableObservable<T>> {
  return (source: Observable<T>) => multicast(new AsyncSubject<T>())(source);
}
