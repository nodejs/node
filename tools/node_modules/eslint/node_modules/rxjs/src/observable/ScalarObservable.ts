import { IScheduler } from '../Scheduler';
import { Observable } from '../Observable';
import { Subscriber } from '../Subscriber';
import { TeardownLogic } from '../Subscription';

/**
 * We need this JSDoc comment for affecting ESDoc.
 * @extends {Ignored}
 * @hide true
 */
export class ScalarObservable<T> extends Observable<T> {
  static create<T>(value: T, scheduler?: IScheduler): ScalarObservable<T> {
    return new ScalarObservable(value, scheduler);
  }

  static dispatch(state: any): void {
    const { done, value, subscriber } = state;

    if (done) {
      subscriber.complete();
      return;
    }

    subscriber.next(value);
    if (subscriber.closed) {
      return;
    }

    state.done = true;
    (<any> this).schedule(state);
  }

  _isScalar: boolean = true;

  constructor(public value: T, private scheduler?: IScheduler) {
    super();
    if (scheduler) {
      this._isScalar = false;
    }
  }

  /** @deprecated internal use only */ _subscribe(subscriber: Subscriber<T>): TeardownLogic {
    const value = this.value;
    const scheduler = this.scheduler;

    if (scheduler) {
      return scheduler.schedule(ScalarObservable.dispatch, 0, {
        done: false, value, subscriber
      });
    } else {
      subscriber.next(value);
      if (!subscriber.closed) {
        subscriber.complete();
      }
    }
  }
}
