import { root } from '../util/root';
import { IScheduler } from '../Scheduler';
import { Observable } from '../Observable';
import { iterator as Symbol_iterator } from '../symbol/iterator';
import { TeardownLogic } from '../Subscription';
import { Subscriber } from '../Subscriber';

/**
 * We need this JSDoc comment for affecting ESDoc.
 * @extends {Ignored}
 * @hide true
 */
export class IteratorObservable<T> extends Observable<T> {
  private iterator: any;

  static create<T>(iterator: any, scheduler?: IScheduler): IteratorObservable<T> {
    return new IteratorObservable(iterator, scheduler);
  }

  static dispatch(state: any) {

    const { index, hasError, iterator, subscriber } = state;

    if (hasError) {
      subscriber.error(state.error);
      return;
    }

    let result = iterator.next();
    if (result.done) {
      subscriber.complete();
      return;
    }

    subscriber.next(result.value);
    state.index = index + 1;

    if (subscriber.closed) {
      if (typeof iterator.return === 'function') {
        iterator.return();
      }
      return;
    }

    (<any> this).schedule(state);
  }

  constructor(iterator: any, private scheduler?: IScheduler) {
    super();

    if (iterator == null) {
      throw new Error('iterator cannot be null.');
    }

    this.iterator = getIterator(iterator);
  }

  /** @deprecated internal use only */ _subscribe(subscriber: Subscriber<T>): TeardownLogic {

    let index = 0;
    const { iterator, scheduler } = this;

    if (scheduler) {
      return scheduler.schedule(IteratorObservable.dispatch, 0, {
        index, iterator, subscriber
      });
    } else {
      do {
        let result = iterator.next();
        if (result.done) {
          subscriber.complete();
          break;
        } else {
          subscriber.next(result.value);
        }
        if (subscriber.closed) {
          if (typeof iterator.return === 'function') {
            iterator.return();
          }
          break;
        }
      } while (true);
    }
  }
}

class StringIterator {
  constructor(private str: string,
              private idx: number = 0,
              private len: number = str.length) {
  }
  [Symbol_iterator]() { return (this); }
  next() {
    return this.idx < this.len ? {
        done: false,
        value: this.str.charAt(this.idx++)
    } : {
        done: true,
        value: undefined
    };
  }
}

class ArrayIterator {
  constructor(private arr: Array<any>,
              private idx: number = 0,
              private len: number = toLength(arr)) {
  }
  [Symbol_iterator]() { return this; }
  next() {
    return this.idx < this.len ? {
        done: false,
        value: this.arr[this.idx++]
    } : {
        done: true,
        value: undefined
    };
  }
}

function getIterator(obj: any) {
  const i = obj[Symbol_iterator];
  if (!i && typeof obj === 'string') {
    return new StringIterator(obj);
  }
  if (!i && obj.length !== undefined) {
    return new ArrayIterator(obj);
  }
  if (!i) {
    throw new TypeError('object is not iterable');
  }
  return obj[Symbol_iterator]();
}

const maxSafeInteger = Math.pow(2, 53) - 1;

function toLength(o: any) {
  let len = +o.length;
  if (isNaN(len)) {
      return 0;
  }
  if (len === 0 || !numberIsFinite(len)) {
      return len;
  }
  len = sign(len) * Math.floor(Math.abs(len));
  if (len <= 0) {
      return 0;
  }
  if (len > maxSafeInteger) {
      return maxSafeInteger;
  }
  return len;
}

function numberIsFinite(value: any) {
  return typeof value === 'number' && root.isFinite(value);
}

function sign(value: any) {
  let valueAsNumber = +value;
  if (valueAsNumber === 0) {
    return valueAsNumber;
  }
  if (isNaN(valueAsNumber)) {
    return valueAsNumber;
  }
  return valueAsNumber < 0 ? -1 : 1;
}
