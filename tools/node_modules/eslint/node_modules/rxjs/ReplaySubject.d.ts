import { Subject } from './Subject';
import { IScheduler } from './Scheduler';
import { Subscriber } from './Subscriber';
import { Subscription } from './Subscription';
/**
 * @class ReplaySubject<T>
 */
export declare class ReplaySubject<T> extends Subject<T> {
    private scheduler;
    private _events;
    private _bufferSize;
    private _windowTime;
    constructor(bufferSize?: number, windowTime?: number, scheduler?: IScheduler);
    next(value: T): void;
    /** @deprecated internal use only */ _subscribe(subscriber: Subscriber<T>): Subscription;
    _getNow(): number;
    private _trimBufferThenGetEvents();
}
