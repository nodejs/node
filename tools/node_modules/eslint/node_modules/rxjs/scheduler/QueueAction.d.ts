import { AsyncAction } from './AsyncAction';
import { Subscription } from '../Subscription';
import { QueueScheduler } from './QueueScheduler';
/**
 * We need this JSDoc comment for affecting ESDoc.
 * @ignore
 * @extends {Ignored}
 */
export declare class QueueAction<T> extends AsyncAction<T> {
    protected scheduler: QueueScheduler;
    protected work: (this: QueueAction<T>, state?: T) => void;
    constructor(scheduler: QueueScheduler, work: (this: QueueAction<T>, state?: T) => void);
    schedule(state?: T, delay?: number): Subscription;
    execute(state: T, delay: number): any;
    protected requestAsyncId(scheduler: QueueScheduler, id?: any, delay?: number): any;
}
