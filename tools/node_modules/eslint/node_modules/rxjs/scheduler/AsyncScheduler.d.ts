import { Scheduler } from '../Scheduler';
import { AsyncAction } from './AsyncAction';
export declare class AsyncScheduler extends Scheduler {
    actions: Array<AsyncAction<any>>;
    /**
     * A flag to indicate whether the Scheduler is currently executing a batch of
     * queued actions.
     * @type {boolean}
     */
    active: boolean;
    /**
     * An internal ID used to track the latest asynchronous task such as those
     * coming from `setTimeout`, `setInterval`, `requestAnimationFrame`, and
     * others.
     * @type {any}
     */
    scheduled: any;
    flush(action: AsyncAction<any>): void;
}
