import { Observable, SubscribableOrPromise } from '../Observable';
import { Subscriber } from '../Subscriber';
import { TeardownLogic } from '../Subscription';

import { subscribeToResult } from '../util/subscribeToResult';
import { OuterSubscriber } from '../OuterSubscriber';
/**
 * We need this JSDoc comment for affecting ESDoc.
 * @extends {Ignored}
 * @hide true
 */
export class IfObservable<T, R> extends Observable<T> {

  static create<T, R>(condition: () => boolean | void,
                      thenSource?: SubscribableOrPromise<T> | void,
                      elseSource?: SubscribableOrPromise<R> | void): Observable<T|R> {
    return new IfObservable(condition, thenSource, elseSource);
  }

  constructor(private condition: () => boolean | void,
              private thenSource?: SubscribableOrPromise<T> | void,
              private elseSource?: SubscribableOrPromise<R> | void) {
    super();
  }

  /** @deprecated internal use only */ _subscribe(subscriber: Subscriber<T|R>): TeardownLogic {
    const { condition, thenSource, elseSource } = this;

    return new IfSubscriber(subscriber, condition, thenSource, elseSource);
  }
}

class IfSubscriber<T, R> extends OuterSubscriber<T, T> {
  constructor(destination: Subscriber<T>,
              private condition: () => boolean | void,
              private thenSource?: SubscribableOrPromise<T> | void,
              private elseSource?: SubscribableOrPromise<R> | void) {
    super(destination);
    this.tryIf();
  }

  private tryIf(): void {
    const { condition, thenSource, elseSource } = this;

    let result: boolean;
    try {
      result = <boolean>condition();
      const source = result ? thenSource : elseSource;

      if (source) {
        this.add(subscribeToResult(this, source));
      } else {
        this._complete();
      }
    } catch (err) {
      this._error(err);
    }
  }
}
