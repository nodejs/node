import { Operator } from '../Operator';
import { Subscriber } from '../Subscriber';
import { Subscription } from '../Subscription';
import { async } from '../scheduler/async';
import { Observable } from '../Observable';
import { ThrottleConfig, defaultThrottleConfig } from './throttle';
import { MonoTypeOperatorFunction, SchedulerLike, TeardownLogic } from '../types';

/**
 * Emits a value from the source Observable, then ignores subsequent source
 * values for `duration` milliseconds, then repeats this process.
 *
 * <span class="informal">Lets a value pass, then ignores source values for the
 * next `duration` milliseconds.</span>
 *
 * ![](throttleTime.png)
 *
 * `throttleTime` emits the source Observable values on the output Observable
 * when its internal timer is disabled, and ignores source values when the timer
 * is enabled. Initially, the timer is disabled. As soon as the first source
 * value arrives, it is forwarded to the output Observable, and then the timer
 * is enabled. After `duration` milliseconds (or the time unit determined
 * internally by the optional `scheduler`) has passed, the timer is disabled,
 * and this process repeats for the next source value. Optionally takes a
 * {@link SchedulerLike} for managing timers.
 *
 * ## Example
 * Emit clicks at a rate of at most one click per second
 * ```javascript
 * import { fromEvent } from 'rxjs';
 * import { throttleTime } from 'rxjs/operators';
 *
 * const clicks = fromEvent(document, 'click');
 * const result = clicks.pipe(throttleTime(1000));
 * result.subscribe(x => console.log(x));
 * ```
 *
 * @see {@link auditTime}
 * @see {@link debounceTime}
 * @see {@link delay}
 * @see {@link sampleTime}
 * @see {@link throttle}
 *
 * @param {number} duration Time to wait before emitting another value after
 * emitting the last value, measured in milliseconds or the time unit determined
 * internally by the optional `scheduler`.
 * @param {SchedulerLike} [scheduler=async] The {@link SchedulerLike} to use for
 * managing the timers that handle the throttling.
 * @param {Object} config a configuration object to define `leading` and
 * `trailing` behavior. Defaults to `{ leading: true, trailing: false }`.
 * @return {Observable<T>} An Observable that performs the throttle operation to
 * limit the rate of emissions from the source.
 * @method throttleTime
 * @owner Observable
 */
export function throttleTime<T>(duration: number,
                                scheduler: SchedulerLike = async,
                                config: ThrottleConfig = defaultThrottleConfig): MonoTypeOperatorFunction<T> {
  return (source: Observable<T>) => source.lift(new ThrottleTimeOperator(duration, scheduler, config.leading, config.trailing));
}

class ThrottleTimeOperator<T> implements Operator<T, T> {
  constructor(private duration: number,
              private scheduler: SchedulerLike,
              private leading: boolean,
              private trailing: boolean) {
  }

  call(subscriber: Subscriber<T>, source: any): TeardownLogic {
    return source.subscribe(
      new ThrottleTimeSubscriber(subscriber, this.duration, this.scheduler, this.leading, this.trailing)
    );
  }
}

/**
 * We need this JSDoc comment for affecting ESDoc.
 * @ignore
 * @extends {Ignored}
 */
class ThrottleTimeSubscriber<T> extends Subscriber<T> {
  private throttled: Subscription;
  private _hasTrailingValue: boolean = false;
  private _trailingValue: T = null;

  constructor(destination: Subscriber<T>,
              private duration: number,
              private scheduler: SchedulerLike,
              private leading: boolean,
              private trailing: boolean) {
    super(destination);
  }

  protected _next(value: T) {
    if (this.throttled) {
      if (this.trailing) {
        this._trailingValue = value;
        this._hasTrailingValue = true;
      }
    } else {
      this.add(this.throttled = this.scheduler.schedule<DispatchArg<T>>(dispatchNext, this.duration, { subscriber: this }));
      if (this.leading) {
        this.destination.next(value);
      }
    }
  }

  protected _complete() {
    if (this._hasTrailingValue) {
      this.destination.next(this._trailingValue);
      this.destination.complete();
    } else {
      this.destination.complete();
    }
  }

  clearThrottle() {
    const throttled = this.throttled;
    if (throttled) {
      if (this.trailing && this._hasTrailingValue) {
        this.destination.next(this._trailingValue);
        this._trailingValue = null;
        this._hasTrailingValue = false;
      }
      throttled.unsubscribe();
      this.remove(throttled);
      this.throttled = null;
    }
  }
}

interface DispatchArg<T> {
  subscriber: ThrottleTimeSubscriber<T>;
}

function dispatchNext<T>(arg: DispatchArg<T>) {
  const { subscriber } = arg;
  subscriber.clearThrottle();
}
