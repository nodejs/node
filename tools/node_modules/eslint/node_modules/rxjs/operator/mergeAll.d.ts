import { Observable } from '../Observable';
import { Subscribable } from '../Observable';
export declare function mergeAll<T>(this: Observable<T>, concurrent?: number): T;
export declare function mergeAll<T, R>(this: Observable<T>, concurrent?: number): Subscribable<R>;
