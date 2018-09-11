import { Subject } from './Subject';
import { SchedulerLike } from './types';
import { Subscriber } from './Subscriber';
import { Subscription } from './Subscription';
/**
 * @class ReplaySubject<T>
 */
export declare class ReplaySubject<T> extends Subject<T> {
    private scheduler?;
    private _events;
    private _bufferSize;
    private _windowTime;
    private _infiniteTimeWindow;
    constructor(bufferSize?: number, windowTime?: number, scheduler?: SchedulerLike);
    private nextInfiniteTimeWindow;
    private nextTimeWindow;
    /** @deprecated This is an internal implementation detail, do not use. */
    _subscribe(subscriber: Subscriber<T>): Subscription;
    _getNow(): number;
    private _trimBufferThenGetEvents;
}
