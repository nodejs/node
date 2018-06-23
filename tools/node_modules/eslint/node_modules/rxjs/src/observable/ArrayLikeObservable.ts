import { IScheduler } from '../Scheduler';
import { Observable } from '../Observable';
import { ScalarObservable } from './ScalarObservable';
import { EmptyObservable } from './EmptyObservable';
import { Subscriber } from '../Subscriber';
import { TeardownLogic } from '../Subscription';

/**
 * We need this JSDoc comment for affecting ESDoc.
 * @extends {Ignored}
 * @hide true
 */
export class ArrayLikeObservable<T> extends Observable<T> {

  static create<T>(arrayLike: ArrayLike<T>, scheduler?: IScheduler): Observable<T> {
    const length = arrayLike.length;
    if (length === 0) {
      return new EmptyObservable<T>();
    } else if (length === 1) {
      return new ScalarObservable<T>(<any>arrayLike[0], scheduler);
    } else {
      return new ArrayLikeObservable(arrayLike, scheduler);
    }
  }

  static dispatch(state: any) {
    const { arrayLike, index, length, subscriber } = state;

    if (subscriber.closed) {
      return;
    }

    if (index >= length) {
      subscriber.complete();
      return;
    }

    subscriber.next(arrayLike[index]);

    state.index = index + 1;

    (<any> this).schedule(state);
  }

  // value used if Array has one value and _isScalar
  private value: any;

  constructor(private arrayLike: ArrayLike<T>, private scheduler?: IScheduler) {
    super();
    if (!scheduler && arrayLike.length === 1) {
      this._isScalar = true;
      this.value = arrayLike[0];
    }
  }

  /** @deprecated internal use only */ _subscribe(subscriber: Subscriber<T>): TeardownLogic {
    let index = 0;
    const { arrayLike, scheduler } = this;
    const length = arrayLike.length;

    if (scheduler) {
      return scheduler.schedule(ArrayLikeObservable.dispatch, 0, {
        arrayLike, index, length, subscriber
      });
    } else {
      for (let i = 0; i < length && !subscriber.closed; i++) {
        subscriber.next(arrayLike[i]);
      }
      subscriber.complete();
    }
  }
}
