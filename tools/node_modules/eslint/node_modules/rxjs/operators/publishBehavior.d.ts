import { Observable } from '../Observable';
import { ConnectableObservable } from '../observable/ConnectableObservable';
import { UnaryFunction } from '../interfaces';
/**
 * @param value
 * @return {ConnectableObservable<T>}
 * @method publishBehavior
 * @owner Observable
 */
export declare function publishBehavior<T>(value: T): UnaryFunction<Observable<T>, ConnectableObservable<T>>;
