import { Action } from '../scheduler/Action';
import { async } from '../scheduler/async';
import { isDate } from '../util/isDate';
import { Operator } from '../Operator';
import { Subscriber } from '../Subscriber';
import { IScheduler } from '../Scheduler';
import { Observable } from '../Observable';
import { TeardownLogic } from '../Subscription';
import { TimeoutError } from '../util/TimeoutError';
import { MonoTypeOperatorFunction } from '../interfaces';

/**
 *
 * Errors if Observable does not emit a value in given time span.
 *
 * <span class="informal">Timeouts on Observable that doesn't emit values fast enough.</span>
 *
 * <img src="./img/timeout.png" width="100%">
 *
 * `timeout` operator accepts as an argument either a number or a Date.
 *
 * If number was provided, it returns an Observable that behaves like a source
 * Observable, unless there is a period of time where there is no value emitted.
 * So if you provide `100` as argument and first value comes after 50ms from
 * the moment of subscription, this value will be simply re-emitted by the resulting
 * Observable. If however after that 100ms passes without a second value being emitted,
 * stream will end with an error and source Observable will be unsubscribed.
 * These checks are performed throughout whole lifecycle of Observable - from the moment
 * it was subscribed to, until it completes or errors itself. Thus every value must be
 * emitted within specified period since previous value.
 *
 * If provided argument was Date, returned Observable behaves differently. It throws
 * if Observable did not complete before provided Date. This means that periods between
 * emission of particular values do not matter in this case. If Observable did not complete
 * before provided Date, source Observable will be unsubscribed. Other than that, resulting
 * stream behaves just as source Observable.
 *
 * `timeout` accepts also a Scheduler as a second parameter. It is used to schedule moment (or moments)
 * when returned Observable will check if source stream emitted value or completed.
 *
 * @example <caption>Check if ticks are emitted within certain timespan</caption>
 * const seconds = Rx.Observable.interval(1000);
 *
 * seconds.timeout(1100) // Let's use bigger timespan to be safe,
 *                       // since `interval` might fire a bit later then scheduled.
 * .subscribe(
 *     value => console.log(value), // Will emit numbers just as regular `interval` would.
 *     err => console.log(err) // Will never be called.
 * );
 *
 * seconds.timeout(900).subscribe(
 *     value => console.log(value), // Will never be called.
 *     err => console.log(err) // Will emit error before even first value is emitted,
 *                             // since it did not arrive within 900ms period.
 * );
 *
 * @example <caption>Use Date to check if Observable completed</caption>
 * const seconds = Rx.Observable.interval(1000);
 *
 * seconds.timeout(new Date("December 17, 2020 03:24:00"))
 * .subscribe(
 *     value => console.log(value), // Will emit values as regular `interval` would
 *                                  // until December 17, 2020 at 03:24:00.
 *     err => console.log(err) // On December 17, 2020 at 03:24:00 it will emit an error,
 *                             // since Observable did not complete by then.
 * );
 *
 * @see {@link timeoutWith}
 *
 * @param {number|Date} due Number specifying period within which Observable must emit values
 *                          or Date specifying before when Observable should complete
 * @param {Scheduler} [scheduler] Scheduler controlling when timeout checks occur.
 * @return {Observable<T>} Observable that mirrors behaviour of source, unless timeout checks fail.
 * @method timeout
 * @owner Observable
 */
export function timeout<T>(due: number | Date,
                           scheduler: IScheduler = async): MonoTypeOperatorFunction<T> {
  const absoluteTimeout = isDate(due);
  const waitFor = absoluteTimeout ? (+due - scheduler.now()) : Math.abs(<number>due);
  return (source: Observable<T>) => source.lift(new TimeoutOperator(waitFor, absoluteTimeout, scheduler, new TimeoutError()));
}

class TimeoutOperator<T> implements Operator<T, T> {
  constructor(private waitFor: number,
              private absoluteTimeout: boolean,
              private scheduler: IScheduler,
              private errorInstance: TimeoutError) {
  }

  call(subscriber: Subscriber<T>, source: any): TeardownLogic {
    return source.subscribe(new TimeoutSubscriber<T>(
      subscriber, this.absoluteTimeout, this.waitFor, this.scheduler, this.errorInstance
    ));
  }
}

/**
 * We need this JSDoc comment for affecting ESDoc.
 * @ignore
 * @extends {Ignored}
 */
class TimeoutSubscriber<T> extends Subscriber<T> {

  private action: Action<TimeoutSubscriber<T>> = null;

  constructor(destination: Subscriber<T>,
              private absoluteTimeout: boolean,
              private waitFor: number,
              private scheduler: IScheduler,
              private errorInstance: TimeoutError) {
    super(destination);
    this.scheduleTimeout();
  }

  private static dispatchTimeout<T>(subscriber: TimeoutSubscriber<T>): void {
    subscriber.error(subscriber.errorInstance);
  }

  private scheduleTimeout(): void {
    const { action } = this;
    if (action) {
      // Recycle the action if we've already scheduled one. All the production
      // Scheduler Actions mutate their state/delay time and return themeselves.
      // VirtualActions are immutable, so they create and return a clone. In this
      // case, we need to set the action reference to the most recent VirtualAction,
      // to ensure that's the one we clone from next time.
      this.action = (<Action<TimeoutSubscriber<T>>> action.schedule(this, this.waitFor));
    } else {
      this.add(this.action = (<Action<TimeoutSubscriber<T>>> this.scheduler.schedule(
        TimeoutSubscriber.dispatchTimeout, this.waitFor, this
      )));
    }
  }

  protected _next(value: T): void {
    if (!this.absoluteTimeout) {
      this.scheduleTimeout();
    }
    super._next(value);
  }

  /** @deprecated internal use only */ _unsubscribe() {
    this.action = null;
    this.scheduler = null;
    this.errorInstance = null;
  }
}
