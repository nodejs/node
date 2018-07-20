import { Operator } from '../Operator';
import { Subscriber } from '../Subscriber';
import { Observable } from '../Observable';
import { PartialObserver } from '../Observer';
import { TeardownLogic } from '../Subscription';
import { MonoTypeOperatorFunction } from '../interfaces';

/* tslint:disable:max-line-length */
export function tap<T>(next: (x: T) => void, error?: (e: any) => void, complete?: () => void): MonoTypeOperatorFunction<T>;
export function tap<T>(observer: PartialObserver<T>): MonoTypeOperatorFunction<T>;
/* tslint:enable:max-line-length */

/**
 * Perform a side effect for every emission on the source Observable, but return
 * an Observable that is identical to the source.
 *
 * <span class="informal">Intercepts each emission on the source and runs a
 * function, but returns an output which is identical to the source as long as errors don't occur.</span>
 *
 * <img src="./img/do.png" width="100%">
 *
 * Returns a mirrored Observable of the source Observable, but modified so that
 * the provided Observer is called to perform a side effect for every value,
 * error, and completion emitted by the source. Any errors that are thrown in
 * the aforementioned Observer or handlers are safely sent down the error path
 * of the output Observable.
 *
 * This operator is useful for debugging your Observables for the correct values
 * or performing other side effects.
 *
 * Note: this is different to a `subscribe` on the Observable. If the Observable
 * returned by `do` is not subscribed, the side effects specified by the
 * Observer will never happen. `do` therefore simply spies on existing
 * execution, it does not trigger an execution to happen like `subscribe` does.
 *
 * @example <caption>Map every click to the clientX position of that click, while also logging the click event</caption>
 * var clicks = Rx.Observable.fromEvent(document, 'click');
 * var positions = clicks
 *   .do(ev => console.log(ev))
 *   .map(ev => ev.clientX);
 * positions.subscribe(x => console.log(x));
 *
 * @see {@link map}
 * @see {@link subscribe}
 *
 * @param {Observer|function} [nextOrObserver] A normal Observer object or a
 * callback for `next`.
 * @param {function} [error] Callback for errors in the source.
 * @param {function} [complete] Callback for the completion of the source.
 * @return {Observable} An Observable identical to the source, but runs the
 * specified Observer or callback(s) for each item.
 * @name tap
 */
export function tap<T>(nextOrObserver?: PartialObserver<T> | ((x: T) => void),
                       error?: (e: any) => void,
                       complete?: () => void): MonoTypeOperatorFunction<T> {
  return function tapOperatorFunction(source: Observable<T>): Observable<T> {
    return source.lift(new DoOperator(nextOrObserver, error, complete));
  };
}

class DoOperator<T> implements Operator<T, T> {
  constructor(private nextOrObserver?: PartialObserver<T> | ((x: T) => void),
              private error?: (e: any) => void,
              private complete?: () => void) {
  }
  call(subscriber: Subscriber<T>, source: any): TeardownLogic {
    return source.subscribe(new DoSubscriber(subscriber, this.nextOrObserver, this.error, this.complete));
  }
}

/**
 * We need this JSDoc comment for affecting ESDoc.
 * @ignore
 * @extends {Ignored}
 */
class DoSubscriber<T> extends Subscriber<T> {

  private safeSubscriber: Subscriber<T>;

  constructor(destination: Subscriber<T>,
              nextOrObserver?: PartialObserver<T> | ((x: T) => void),
              error?: (e: any) => void,
              complete?: () => void) {
    super(destination);

    const safeSubscriber = new Subscriber<T>(nextOrObserver, error, complete);
    safeSubscriber.syncErrorThrowable = true;
    this.add(safeSubscriber);
    this.safeSubscriber = safeSubscriber;
  }

  protected _next(value: T): void {
    const { safeSubscriber } = this;
    safeSubscriber.next(value);
    if (safeSubscriber.syncErrorThrown) {
      this.destination.error(safeSubscriber.syncErrorValue);
    } else {
      this.destination.next(value);
    }
  }

  protected _error(err: any): void {
    const { safeSubscriber } = this;
    safeSubscriber.error(err);
    if (safeSubscriber.syncErrorThrown) {
      this.destination.error(safeSubscriber.syncErrorValue);
    } else {
      this.destination.error(err);
    }
  }

  protected _complete(): void {
    const { safeSubscriber } = this;
    safeSubscriber.complete();
    if (safeSubscriber.syncErrorThrown) {
      this.destination.error(safeSubscriber.syncErrorValue);
    } else {
      this.destination.complete();
    }
  }
}
