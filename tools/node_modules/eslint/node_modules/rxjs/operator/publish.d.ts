import { Observable } from '../Observable';
import { ConnectableObservable } from '../observable/ConnectableObservable';
export declare function publish<T>(this: Observable<T>): ConnectableObservable<T>;
export declare function publish<T>(this: Observable<T>, selector: (source: Observable<T>) => Observable<T>): Observable<T>;
export declare function publish<T, R>(this: Observable<T>, selector: (source: Observable<T>) => Observable<R>): Observable<R>;
export declare type selector<T> = (source: Observable<T>) => Observable<T>;
