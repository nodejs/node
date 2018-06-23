import { Subject } from '../Subject';
import { Observable } from '../Observable';
import { ConnectableObservable } from '../observable/ConnectableObservable';
import { FactoryOrValue, MonoTypeOperatorFunction, OperatorFunction } from '../interfaces';
export declare function multicast<T>(this: Observable<T>, subjectOrSubjectFactory: FactoryOrValue<Subject<T>>): ConnectableObservable<T>;
export declare function multicast<T>(SubjectFactory: (this: Observable<T>) => Subject<T>, selector?: MonoTypeOperatorFunction<T>): Observable<T>;
export declare function multicast<T, R>(SubjectFactory: (this: Observable<T>) => Subject<T>, selector?: OperatorFunction<T, R>): Observable<R>;
