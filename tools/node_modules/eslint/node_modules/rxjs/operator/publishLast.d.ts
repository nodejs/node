import { Observable } from '../Observable';
import { ConnectableObservable } from '../observable/ConnectableObservable';
/**
 * @return {ConnectableObservable<T>}
 * @method publishLast
 * @owner Observable
 */
export declare function publishLast<T>(this: Observable<T>): ConnectableObservable<T>;
