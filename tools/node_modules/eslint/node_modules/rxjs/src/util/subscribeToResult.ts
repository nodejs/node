import { root } from './root';
import { isArrayLike } from './isArrayLike';
import { isPromise } from './isPromise';
import { isObject } from './isObject';
import { Subscriber } from '../Subscriber';
import { Observable, ObservableInput } from '../Observable';
import { iterator as Symbol_iterator } from '../symbol/iterator';
import { Subscription } from '../Subscription';
import { InnerSubscriber } from '../InnerSubscriber';
import { OuterSubscriber } from '../OuterSubscriber';
import { observable as Symbol_observable } from '../symbol/observable';

export function subscribeToResult<T, R>(outerSubscriber: OuterSubscriber<T, R>,
                                        result: any,
                                        outerValue?: T,
                                        outerIndex?: number): Subscription;
export function subscribeToResult<T>(outerSubscriber: OuterSubscriber<any, any>,
                                     result: ObservableInput<T>,
                                     outerValue?: T,
                                     outerIndex?: number): Subscription {
  let destination: Subscriber<any> = new InnerSubscriber(outerSubscriber, outerValue, outerIndex);

  if (destination.closed) {
    return null;
  }

  if (result instanceof Observable) {
    if (result._isScalar) {
      destination.next((<any>result).value);
      destination.complete();
      return null;
    } else {
      destination.syncErrorThrowable = true;
      return result.subscribe(destination);
    }
  } else if (isArrayLike(result)) {
    for (let i = 0, len = result.length; i < len && !destination.closed; i++) {
      destination.next(result[i]);
    }
    if (!destination.closed) {
      destination.complete();
    }
  } else if (isPromise(result)) {
    result.then(
      (value) => {
        if (!destination.closed) {
          destination.next(<any>value);
          destination.complete();
        }
      },
      (err: any) => destination.error(err)
    )
    .then(null, (err: any) => {
      // Escaping the Promise trap: globally throw unhandled errors
      root.setTimeout(() => { throw err; });
    });
    return destination;
  } else if (result && typeof result[Symbol_iterator] === 'function') {
    const iterator = <any>result[Symbol_iterator]();
    do {
      let item = iterator.next();
      if (item.done) {
        destination.complete();
        break;
      }
      destination.next(item.value);
      if (destination.closed) {
        break;
      }
    } while (true);
  } else if (result && typeof result[Symbol_observable] === 'function') {
    const obs = result[Symbol_observable]();
    if (typeof obs.subscribe !== 'function') {
      destination.error(new TypeError('Provided object does not correctly implement Symbol.observable'));
    } else {
      return obs.subscribe(new InnerSubscriber(outerSubscriber, outerValue, outerIndex));
    }
  } else {
    const value = isObject(result) ? 'an invalid object' : `'${result}'`;
    const msg = `You provided ${value} where a stream was expected.`
      + ' You can provide an Observable, Promise, Array, or Iterable.';
    destination.error(new TypeError(msg));
  }
  return null;
}
