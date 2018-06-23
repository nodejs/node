import { Observable } from '../Observable';
import { Subscribable } from '../Observable';
export declare function concatAll<T>(this: Observable<T>): T;
export declare function concatAll<T, R>(this: Observable<T>): Subscribable<R>;
