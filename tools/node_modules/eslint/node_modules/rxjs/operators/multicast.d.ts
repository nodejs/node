import { Subject } from '../Subject';
import { Operator } from '../Operator';
import { Subscriber } from '../Subscriber';
import { Observable } from '../Observable';
import { ConnectableObservable } from '../observable/ConnectableObservable';
import { FactoryOrValue, MonoTypeOperatorFunction, OperatorFunction, UnaryFunction } from '../interfaces';
export declare function multicast<T>(subjectOrSubjectFactory: FactoryOrValue<Subject<T>>): UnaryFunction<Observable<T>, ConnectableObservable<T>>;
export declare function multicast<T>(SubjectFactory: (this: Observable<T>) => Subject<T>, selector?: MonoTypeOperatorFunction<T>): MonoTypeOperatorFunction<T>;
export declare function multicast<T, R>(SubjectFactory: (this: Observable<T>) => Subject<T>, selector?: OperatorFunction<T, R>): OperatorFunction<T, R>;
export declare class MulticastOperator<T, R> implements Operator<T, R> {
    private subjectFactory;
    private selector;
    constructor(subjectFactory: () => Subject<T>, selector: (source: Observable<T>) => Observable<R>);
    call(subscriber: Subscriber<R>, source: any): any;
}
