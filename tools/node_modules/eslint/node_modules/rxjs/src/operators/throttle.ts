import { Operator } from '../Operator';
import { Observable, SubscribableOrPromise } from '../Observable';
import { Subscriber } from '../Subscriber';
import { Subscription, TeardownLogic } from '../Subscription';

import { OuterSubscriber } from '../OuterSubscriber';
import { InnerSubscriber } from '../InnerSubscriber';
import { subscribeToResult } from '../util/subscribeToResult';

import { MonoTypeOperatorFunction } from '../interfaces';

export interface ThrottleConfig {
  leading?: boolean;
  trailing?: boolean;
}

export const defaultThrottleConfig: ThrottleConfig = {
  leading: true,
  trailing: false
};

/**
 * Emits a value from the source Observable, then ignores subsequent source
 * values for a duration determined by another Observable, then repeats this
 * process.
 *
 * <span class="informal">It's like {@link throttleTime}, but the silencing
 * duration is determined by a second Observable.</span>
 *
 * <img src="./img/throttle.png" width="100%">
 *
 * `throttle` emits the source Observable values on the output Observable
 * when its internal timer is disabled, and ignores source values when the timer
 * is enabled. Initially, the timer is disabled. As soon as the first source
 * value arrives, it is forwarded to the output Observable, and then the timer
 * is enabled by calling the `durationSelector` function with the source value,
 * which returns the "duration" Observable. When the duration Observable emits a
 * value or completes, the timer is disabled, and this process repeats for the
 * next source value.
 *
 * @example <caption>Emit clicks at a rate of at most one click per second</caption>
 * var clicks = Rx.Observable.fromEvent(document, 'click');
 * var result = clicks.throttle(ev => Rx.Observable.interval(1000));
 * result.subscribe(x => console.log(x));
 *
 * @see {@link audit}
 * @see {@link debounce}
 * @see {@link delayWhen}
 * @see {@link sample}
 * @see {@link throttleTime}
 *
 * @param {function(value: T): SubscribableOrPromise} durationSelector A function
 * that receives a value from the source Observable, for computing the silencing
 * duration for each source value, returned as an Observable or a Promise.
 * @param {Object} config a configuration object to define `leading` and `trailing` behavior. Defaults
 * to `{ leading: true, trailing: false }`.
 * @return {Observable<T>} An Observable that performs the throttle operation to
 * limit the rate of emissions from the source.
 * @method throttle
 * @owner Observable
 */
export function throttle<T>(durationSelector: (value: T) => SubscribableOrPromise<number>,
                            config: ThrottleConfig = defaultThrottleConfig): MonoTypeOperatorFunction<T> {
  return (source: Observable<T>) => source.lift(new ThrottleOperator(durationSelector, config.leading, config.trailing));
}

class ThrottleOperator<T> implements Operator<T, T> {
  constructor(private durationSelector: (value: T) => SubscribableOrPromise<number>,
              private leading: boolean,
              private trailing: boolean) {
  }

  call(subscriber: Subscriber<T>, source: any): TeardownLogic {
    return source.subscribe(
      new ThrottleSubscriber(subscriber, this.durationSelector, this.leading, this.trailing)
    );
  }
}

/**
 * We need this JSDoc comment for affecting ESDoc
 * @ignore
 * @extends {Ignored}
 */
class ThrottleSubscriber<T, R> extends OuterSubscriber<T, R> {
  private throttled: Subscription;
  private _trailingValue: T;
  private _hasTrailingValue = false;

  constructor(protected destination: Subscriber<T>,
              private durationSelector: (value: T) => SubscribableOrPromise<number>,
              private _leading: boolean,
              private _trailing: boolean) {
    super(destination);
  }

  protected _next(value: T): void {
    if (this.throttled) {
      if (this._trailing) {
        this._hasTrailingValue = true;
        this._trailingValue = value;
      }
    } else {
      const duration = this.tryDurationSelector(value);
      if (duration) {
        this.add(this.throttled = subscribeToResult(this, duration));
      }
      if (this._leading) {
        this.destination.next(value);
        if (this._trailing) {
          this._hasTrailingValue = true;
          this._trailingValue = value;
        }
      }
    }
  }

  private tryDurationSelector(value: T): SubscribableOrPromise<any> {
    try {
      return this.durationSelector(value);
    } catch (err) {
      this.destination.error(err);
      return null;
    }
  }

  /** @deprecated internal use only */ _unsubscribe() {
    const { throttled, _trailingValue, _hasTrailingValue, _trailing } = this;

    this._trailingValue = null;
    this._hasTrailingValue = false;

    if (throttled) {
      this.remove(throttled);
      this.throttled = null;
      throttled.unsubscribe();
    }
  }

  private _sendTrailing() {
    const { destination, throttled, _trailing, _trailingValue, _hasTrailingValue } = this;
    if (throttled && _trailing && _hasTrailingValue) {
      destination.next(_trailingValue);
      this._trailingValue = null;
      this._hasTrailingValue = false;
    }
  }

  notifyNext(outerValue: T, innerValue: R,
             outerIndex: number, innerIndex: number,
             innerSub: InnerSubscriber<T, R>): void {
    this._sendTrailing();
    this._unsubscribe();
  }

  notifyComplete(): void {
    this._sendTrailing();
    this._unsubscribe();
  }
}
