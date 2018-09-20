import { Observable } from '../Observable';
export declare function fromEventPattern<T>(addHandler: (handler: Function) => any, removeHandler?: (handler: Function, signal?: any) => void): Observable<T>;
/** @deprecated resultSelector no longer supported, pipe to map instead */
export declare function fromEventPattern<T>(addHandler: (handler: Function) => any, removeHandler?: (handler: Function, signal?: any) => void, resultSelector?: (...args: any[]) => T): Observable<T>;
