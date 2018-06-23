import { Observable, ObservableInput } from '../Observable';
import { Operator } from '../Operator';
import { Subscriber } from '../Subscriber';
import { Subscription } from '../Subscription';
import { OuterSubscriber } from '../OuterSubscriber';
import { InnerSubscriber } from '../InnerSubscriber';
import { OperatorFunction } from '../interfaces';
export declare function mergeMapTo<T, R>(observable: ObservableInput<R>, concurrent?: number): OperatorFunction<T, R>;
export declare function mergeMapTo<T, I, R>(observable: ObservableInput<I>, resultSelector: (outerValue: T, innerValue: I, outerIndex: number, innerIndex: number) => R, concurrent?: number): OperatorFunction<T, R>;
export declare class MergeMapToOperator<T, I, R> implements Operator<Observable<T>, R> {
    private ish;
    private resultSelector;
    private concurrent;
    constructor(ish: ObservableInput<I>, resultSelector?: (outerValue: T, innerValue: I, outerIndex: number, innerIndex: number) => R, concurrent?: number);
    call(observer: Subscriber<R>, source: any): any;
}
/**
 * We need this JSDoc comment for affecting ESDoc.
 * @ignore
 * @extends {Ignored}
 */
export declare class MergeMapToSubscriber<T, I, R> extends OuterSubscriber<T, I> {
    private ish;
    private resultSelector;
    private concurrent;
    private hasCompleted;
    private buffer;
    private active;
    protected index: number;
    constructor(destination: Subscriber<R>, ish: ObservableInput<I>, resultSelector?: (outerValue: T, innerValue: I, outerIndex: number, innerIndex: number) => R, concurrent?: number);
    protected _next(value: T): void;
    private _innerSub(ish, destination, resultSelector, value, index);
    protected _complete(): void;
    notifyNext(outerValue: T, innerValue: I, outerIndex: number, innerIndex: number, innerSub: InnerSubscriber<T, I>): void;
    private trySelectResult(outerValue, innerValue, outerIndex, innerIndex);
    notifyError(err: any): void;
    notifyComplete(innerSub: Subscription): void;
}
