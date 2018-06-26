import { Observable } from '../Observable';
import { ConnectableObservable } from '../observable/ConnectableObservable';
/**
 * @param value
 * @return {ConnectableObservable<T>}
 * @method publishBehavior
 * @owner Observable
 */
export declare function publishBehavior<T>(this: Observable<T>, value: T): ConnectableObservable<T>;
