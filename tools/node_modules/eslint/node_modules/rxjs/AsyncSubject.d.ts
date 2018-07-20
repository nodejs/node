import { Subject } from './Subject';
import { Subscriber } from './Subscriber';
import { Subscription } from './Subscription';
/**
 * @class AsyncSubject<T>
 */
export declare class AsyncSubject<T> extends Subject<T> {
    private value;
    private hasNext;
    private hasCompleted;
    /** @deprecated internal use only */ _subscribe(subscriber: Subscriber<any>): Subscription;
    next(value: T): void;
    error(error: any): void;
    complete(): void;
}
