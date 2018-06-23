import { Observable } from '../Observable';
import { ConnectableObservable } from '../observable/ConnectableObservable';
import { publishBehavior as higherOrder } from '../operators/publishBehavior';

/**
 * @param value
 * @return {ConnectableObservable<T>}
 * @method publishBehavior
 * @owner Observable
 */
export function publishBehavior<T>(this: Observable<T>, value: T): ConnectableObservable<T> {
  return higherOrder(value)(this);
}
