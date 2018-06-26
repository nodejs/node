import { IScheduler } from '../Scheduler';
import { Observable } from '../Observable';
import { TeardownLogic } from '../Subscription';
import { Subscriber } from '../Subscriber';

/**
 * We need this JSDoc comment for affecting ESDoc.
 * @extends {Ignored}
 * @hide true
 */
export class RangeObservable extends Observable<number> {

  /**
   * Creates an Observable that emits a sequence of numbers within a specified
   * range.
   *
   * <span class="informal">Emits a sequence of numbers in a range.</span>
   *
   * <img src="./img/range.png" width="100%">
   *
   * `range` operator emits a range of sequential integers, in order, where you
   * select the `start` of the range and its `length`. By default, uses no
   * IScheduler and just delivers the notifications synchronously, but may use
   * an optional IScheduler to regulate those deliveries.
   *
   * @example <caption>Emits the numbers 1 to 10</caption>
   * var numbers = Rx.Observable.range(1, 10);
   * numbers.subscribe(x => console.log(x));
   *
   * @see {@link timer}
   * @see {@link interval}
   *
   * @param {number} [start=0] The value of the first integer in the sequence.
   * @param {number} [count=0] The number of sequential integers to generate.
   * @param {Scheduler} [scheduler] A {@link IScheduler} to use for scheduling
   * the emissions of the notifications.
   * @return {Observable} An Observable of numbers that emits a finite range of
   * sequential integers.
   * @static true
   * @name range
   * @owner Observable
   */
  static create(start: number = 0,
                count: number = 0,
                scheduler?: IScheduler): Observable<number> {
    return new RangeObservable(start, count, scheduler);
  }

  static dispatch(state: any) {

    const { start, index, count, subscriber } = state;

    if (index >= count) {
      subscriber.complete();
      return;
    }

    subscriber.next(start);

    if (subscriber.closed) {
      return;
    }

    state.index = index + 1;
    state.start = start + 1;

    (<any> this).schedule(state);
  }

  private start: number;
  private _count: number;
  private scheduler: IScheduler;

  constructor(start: number,
              count: number,
              scheduler?: IScheduler) {
    super();
    this.start = start;
    this._count = count;
    this.scheduler = scheduler;
  }

  /** @deprecated internal use only */ _subscribe(subscriber: Subscriber<number>): TeardownLogic {
    let index = 0;
    let start = this.start;
    const count = this._count;
    const scheduler = this.scheduler;

    if (scheduler) {
      return scheduler.schedule(RangeObservable.dispatch, 0, {
        index, count, start, subscriber
      });
    } else {
      do {
        if (index++ >= count) {
          subscriber.complete();
          break;
        }
        subscriber.next(start++);
        if (subscriber.closed) {
          break;
        }
      } while (true);
    }
  }
}
