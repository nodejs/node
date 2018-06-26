import { IScheduler } from '../Scheduler';
import { Observable } from '../Observable';
import { ScalarObservable } from './ScalarObservable';
import { EmptyObservable } from './EmptyObservable';
import { Subscriber } from '../Subscriber';
import { isScheduler } from '../util/isScheduler';
import { TeardownLogic } from '../Subscription';

/**
 * We need this JSDoc comment for affecting ESDoc.
 * @extends {Ignored}
 * @hide true
 */
export class ArrayObservable<T> extends Observable<T> {

  static create<T>(array: T[], scheduler?: IScheduler): Observable<T> {
    return new ArrayObservable(array, scheduler);
  }

  static of<T>(item1: T, scheduler?: IScheduler): Observable<T>;
  static of<T>(item1: T, item2: T, scheduler?: IScheduler): Observable<T>;
  static of<T>(item1: T, item2: T, item3: T, scheduler?: IScheduler): Observable<T>;
  static of<T>(item1: T, item2: T, item3: T, item4: T, scheduler?: IScheduler): Observable<T>;
  static of<T>(item1: T, item2: T, item3: T, item4: T, item5: T, scheduler?: IScheduler): Observable<T>;
  static of<T>(item1: T, item2: T, item3: T, item4: T, item5: T, item6: T, scheduler?: IScheduler): Observable<T>;
  static of<T>(...array: Array<T | IScheduler>): Observable<T>;
  /**
   * Creates an Observable that emits some values you specify as arguments,
   * immediately one after the other, and then emits a complete notification.
   *
   * <span class="informal">Emits the arguments you provide, then completes.
   * </span>
   *
   * <img src="./img/of.png" width="100%">
   *
   * This static operator is useful for creating a simple Observable that only
   * emits the arguments given, and the complete notification thereafter. It can
   * be used for composing with other Observables, such as with {@link concat}.
   * By default, it uses a `null` IScheduler, which means the `next`
   * notifications are sent synchronously, although with a different IScheduler
   * it is possible to determine when those notifications will be delivered.
   *
   * @example <caption>Emit 10, 20, 30, then 'a', 'b', 'c', then start ticking every second.</caption>
   * var numbers = Rx.Observable.of(10, 20, 30);
   * var letters = Rx.Observable.of('a', 'b', 'c');
   * var interval = Rx.Observable.interval(1000);
   * var result = numbers.concat(letters).concat(interval);
   * result.subscribe(x => console.log(x));
   *
   * @see {@link create}
   * @see {@link empty}
   * @see {@link never}
   * @see {@link throw}
   *
   * @param {...T} values Arguments that represent `next` values to be emitted.
   * @param {Scheduler} [scheduler] A {@link IScheduler} to use for scheduling
   * the emissions of the `next` notifications.
   * @return {Observable<T>} An Observable that emits each given input value.
   * @static true
   * @name of
   * @owner Observable
   */
  static of<T>(...array: Array<T | IScheduler>): Observable<T> {
    let scheduler = <IScheduler>array[array.length - 1];
    if (isScheduler(scheduler)) {
      array.pop();
    } else {
      scheduler = null;
    }

    const len = array.length;
    if (len > 1) {
      return new ArrayObservable<T>(<any>array, scheduler);
    } else if (len === 1) {
      return new ScalarObservable<T>(<any>array[0], scheduler);
    } else {
      return new EmptyObservable<T>(scheduler);
    }
  }

  static dispatch(state: any) {

    const { array, index, count, subscriber } = state;

    if (index >= count) {
      subscriber.complete();
      return;
    }

    subscriber.next(array[index]);

    if (subscriber.closed) {
      return;
    }

    state.index = index + 1;

    (<any> this).schedule(state);
  }

  // value used if Array has one value and _isScalar
  value: any;

  constructor(private array: T[], private scheduler?: IScheduler) {
    super();
    if (!scheduler && array.length === 1) {
      this._isScalar = true;
      this.value = array[0];
    }
  }

  /** @deprecated internal use only */ _subscribe(subscriber: Subscriber<T>): TeardownLogic {
    let index = 0;
    const array = this.array;
    const count = array.length;
    const scheduler = this.scheduler;

    if (scheduler) {
      return scheduler.schedule(ArrayObservable.dispatch, 0, {
        array, index, count, subscriber
      });
    } else {
      for (let i = 0; i < count && !subscriber.closed; i++) {
        subscriber.next(array[i]);
      }
      subscriber.complete();
    }
  }
}
