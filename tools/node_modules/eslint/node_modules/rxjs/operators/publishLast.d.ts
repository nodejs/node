import { Observable } from '../Observable';
import { ConnectableObservable } from '../observable/ConnectableObservable';
import { UnaryFunction } from '../interfaces';
export declare function publishLast<T>(): UnaryFunction<Observable<T>, ConnectableObservable<T>>;
