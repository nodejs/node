import { Observable } from '../Observable';
import { ConnectableObservable } from '../observable/ConnectableObservable';
import { publishLast as higherOrder } from '../operators/publishLast';
/**
 * @return {ConnectableObservable<T>}
 * @method publishLast
 * @owner Observable
 */
export function publishLast<T>(this: Observable<T>): ConnectableObservable<T> {
  //TODO(benlesh): correct type-flow through here.
  return higherOrder()(this) as ConnectableObservable<T>;
}
