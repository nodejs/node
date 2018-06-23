import { PartialObserver } from '../Observer';
import { MonoTypeOperatorFunction } from '../interfaces';
export declare function tap<T>(next: (x: T) => void, error?: (e: any) => void, complete?: () => void): MonoTypeOperatorFunction<T>;
export declare function tap<T>(observer: PartialObserver<T>): MonoTypeOperatorFunction<T>;
