import { Observable } from '../Observable';
import { MonoTypeOperatorFunction, OperatorFunction } from '../interfaces';
export declare function race<T>(observables: Array<Observable<T>>): MonoTypeOperatorFunction<T>;
export declare function race<T, R>(observables: Array<Observable<T>>): OperatorFunction<T, R>;
export declare function race<T>(...observables: Array<Observable<T> | Array<Observable<T>>>): MonoTypeOperatorFunction<T>;
export declare function race<T, R>(...observables: Array<Observable<any> | Array<Observable<any>>>): OperatorFunction<T, R>;
