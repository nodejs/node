import { AsyncAction } from './AsyncAction';
import { AsapScheduler } from './AsapScheduler';
/**
 * We need this JSDoc comment for affecting ESDoc.
 * @ignore
 * @extends {Ignored}
 */
export declare class AsapAction<T> extends AsyncAction<T> {
    protected scheduler: AsapScheduler;
    protected work: (this: AsapAction<T>, state?: T) => void;
    constructor(scheduler: AsapScheduler, work: (this: AsapAction<T>, state?: T) => void);
    protected requestAsyncId(scheduler: AsapScheduler, id?: any, delay?: number): any;
    protected recycleAsyncId(scheduler: AsapScheduler, id?: any, delay?: number): any;
}
