import { isNumeric } from '../util/isNumeric';
import { Observable } from '../Observable';
import { async } from '../scheduler/async';
import { isScheduler } from '../util/isScheduler';
import { isDate } from '../util/isDate';
/**
 * We need this JSDoc comment for affecting ESDoc.
 * @extends {Ignored}
 * @hide true
 */
export class TimerObservable extends Observable {
    constructor(dueTime = 0, period, scheduler) {
        super();
        this.period = -1;
        this.dueTime = 0;
        if (isNumeric(period)) {
            this.period = Number(period) < 1 && 1 || Number(period);
        }
        else if (isScheduler(period)) {
            scheduler = period;
        }
        if (!isScheduler(scheduler)) {
            scheduler = async;
        }
        this.scheduler = scheduler;
        this.dueTime = isDate(dueTime) ?
            (+dueTime - this.scheduler.now()) :
            dueTime;
    }
    /**
     * Creates an Observable that starts emitting after an `initialDelay` and
     * emits ever increasing numbers after each `period` of time thereafter.
     *
     * <span class="informal">Its like {@link interval}, but you can specify when
     * should the emissions start.</span>
     *
     * <img src="./img/timer.png" width="100%">
     *
     * `timer` returns an Observable that emits an infinite sequence of ascending
     * integers, with a constant interval of time, `period` of your choosing
     * between those emissions. The first emission happens after the specified
     * `initialDelay`. The initial delay may be a {@link Date}. By default, this
     * operator uses the `async` IScheduler to provide a notion of time, but you
     * may pass any IScheduler to it. If `period` is not specified, the output
     * Observable emits only one value, `0`. Otherwise, it emits an infinite
     * sequence.
     *
     * @example <caption>Emits ascending numbers, one every second (1000ms), starting after 3 seconds</caption>
     * var numbers = Rx.Observable.timer(3000, 1000);
     * numbers.subscribe(x => console.log(x));
     *
     * @example <caption>Emits one number after five seconds</caption>
     * var numbers = Rx.Observable.timer(5000);
     * numbers.subscribe(x => console.log(x));
     *
     * @see {@link interval}
     * @see {@link delay}
     *
     * @param {number|Date} initialDelay The initial delay time to wait before
     * emitting the first value of `0`.
     * @param {number} [period] The period of time between emissions of the
     * subsequent numbers.
     * @param {Scheduler} [scheduler=async] The IScheduler to use for scheduling
     * the emission of values, and providing a notion of "time".
     * @return {Observable} An Observable that emits a `0` after the
     * `initialDelay` and ever increasing numbers after each `period` of time
     * thereafter.
     * @static true
     * @name timer
     * @owner Observable
     */
    static create(initialDelay = 0, period, scheduler) {
        return new TimerObservable(initialDelay, period, scheduler);
    }
    static dispatch(state) {
        const { index, period, subscriber } = state;
        const action = this;
        subscriber.next(index);
        if (subscriber.closed) {
            return;
        }
        else if (period === -1) {
            return subscriber.complete();
        }
        state.index = index + 1;
        action.schedule(state, period);
    }
    /** @deprecated internal use only */ _subscribe(subscriber) {
        const index = 0;
        const { period, dueTime, scheduler } = this;
        return scheduler.schedule(TimerObservable.dispatch, dueTime, {
            index, period, subscriber
        });
    }
}
//# sourceMappingURL=TimerObservable.js.map