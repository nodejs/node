import { IScheduler } from '../Scheduler';
import { Action } from '../scheduler/Action';
import { Observable } from '../Observable';
import { Subscriber } from '../Subscriber';
import { TeardownLogic } from '../Subscription';

interface PairsContext<T> {
  obj: Object;
  keys: Array<string>;
  length: number;
  index: number;
  subscriber: Subscriber<Array<string | T>>;
}

function dispatch<T>(this: Action<PairsContext<T>>, state: PairsContext<T>) {
  const {obj, keys, length, index, subscriber} = state;

  if (index === length) {
    subscriber.complete();
    return;
  }

  const key = keys[index];
  subscriber.next([key, obj[key]]);

  state.index = index + 1;

  this.schedule(state);
}

/**
 * We need this JSDoc comment for affecting ESDoc.
 * @extends {Ignored}
 * @hide true
 */
export class PairsObservable<T> extends Observable<Array<string | T>> {
  private keys: Array<string>;

  /**
   * Convert an object into an observable sequence of [key, value] pairs
   * using an optional IScheduler to enumerate the object.
   *
   * @example <caption>Converts a javascript object to an Observable</caption>
   * var obj = {
   *   foo: 42,
   *   bar: 56,
   *   baz: 78
   * };
   *
   * var source = Rx.Observable.pairs(obj);
   *
   * var subscription = source.subscribe(
   *   function (x) {
   *     console.log('Next: %s', x);
   *   },
   *   function (err) {
   *     console.log('Error: %s', err);
   *   },
   *   function () {
   *     console.log('Completed');
   *   });
   *
   * @param {Object} obj The object to inspect and turn into an
   * Observable sequence.
   * @param {Scheduler} [scheduler] An optional IScheduler to run the
   * enumeration of the input sequence on.
   * @returns {(Observable<Array<string | T>>)} An observable sequence of
   * [key, value] pairs from the object.
   */
  static create<T>(obj: Object, scheduler?: IScheduler): Observable<Array<string | T>> {
    return new PairsObservable<T>(obj, scheduler);
  }

  constructor(private obj: Object, private scheduler?: IScheduler) {
    super();
    this.keys = Object.keys(obj);
  }

  /** @deprecated internal use only */ _subscribe(subscriber: Subscriber<Array<string | T>>): TeardownLogic {
    const {keys, scheduler} = this;
    const length = keys.length;

    if (scheduler) {
      return scheduler.schedule(dispatch, 0, {
        obj: this.obj, keys, length, index: 0, subscriber
      });
    } else {
      for (let idx = 0; idx < length; idx++) {
        const key = keys[idx];
        subscriber.next([key, this.obj[key]]);
      }
      subscriber.complete();
    }
  }
}
