import { Operator } from '../Operator';
import { Observable, ObservableInput } from '../Observable';
import { Subscriber } from '../Subscriber';
import { Subscription } from '../Subscription';
import { OuterSubscriber } from '../OuterSubscriber';
import { InnerSubscriber } from '../InnerSubscriber';
import { subscribeToResult } from '../util/subscribeToResult';
import { OperatorFunction } from '../interfaces';

/* tslint:disable:max-line-length */
export function switchMapTo<T, R>(observable: ObservableInput<R>): OperatorFunction<T, R>;
export function switchMapTo<T, I, R>(observable: ObservableInput<I>, resultSelector: (outerValue: T, innerValue: I, outerIndex: number, innerIndex: number) => R): OperatorFunction<T, R>;
/* tslint:enable:max-line-length */

/**
 * Projects each source value to the same Observable which is flattened multiple
 * times with {@link switch} in the output Observable.
 *
 * <span class="informal">It's like {@link switchMap}, but maps each value
 * always to the same inner Observable.</span>
 *
 * <img src="./img/switchMapTo.png" width="100%">
 *
 * Maps each source value to the given Observable `innerObservable` regardless
 * of the source value, and then flattens those resulting Observables into one
 * single Observable, which is the output Observable. The output Observables
 * emits values only from the most recently emitted instance of
 * `innerObservable`.
 *
 * @example <caption>Rerun an interval Observable on every click event</caption>
 * var clicks = Rx.Observable.fromEvent(document, 'click');
 * var result = clicks.switchMapTo(Rx.Observable.interval(1000));
 * result.subscribe(x => console.log(x));
 *
 * @see {@link concatMapTo}
 * @see {@link switch}
 * @see {@link switchMap}
 * @see {@link mergeMapTo}
 *
 * @param {ObservableInput} innerObservable An Observable to replace each value from
 * the source Observable.
 * @param {function(outerValue: T, innerValue: I, outerIndex: number, innerIndex: number): any} [resultSelector]
 * A function to produce the value on the output Observable based on the values
 * and the indices of the source (outer) emission and the inner Observable
 * emission. The arguments passed to this function are:
 * - `outerValue`: the value that came from the source
 * - `innerValue`: the value that came from the projected Observable
 * - `outerIndex`: the "index" of the value that came from the source
 * - `innerIndex`: the "index" of the value from the projected Observable
 * @return {Observable} An Observable that emits items from the given
 * `innerObservable` (and optionally transformed through `resultSelector`) every
 * time a value is emitted on the source Observable, and taking only the values
 * from the most recently projected inner Observable.
 * @method switchMapTo
 * @owner Observable
 */
export function switchMapTo<T, I, R>(innerObservable: Observable<I>,
                                     resultSelector?: (outerValue: T,
                                                       innerValue: I,
                                                       outerIndex: number,
                                                       innerIndex: number) => R): OperatorFunction<T, I | R> {
  return (source: Observable<T>) => source.lift(new SwitchMapToOperator(innerObservable, resultSelector));
}

class SwitchMapToOperator<T, I, R> implements Operator<T, I> {
  constructor(private observable: Observable<I>,
              private resultSelector?: (outerValue: T, innerValue: I, outerIndex: number, innerIndex: number) => R) {
  }

  call(subscriber: Subscriber<I>, source: any): any {
    return source.subscribe(new SwitchMapToSubscriber(subscriber, this.observable, this.resultSelector));
  }
}

/**
 * We need this JSDoc comment for affecting ESDoc.
 * @ignore
 * @extends {Ignored}
 */
class SwitchMapToSubscriber<T, I, R> extends OuterSubscriber<T, I> {
  private index: number = 0;
  private innerSubscription: Subscription;

  constructor(destination: Subscriber<I>,
              private inner: Observable<I>,
              private resultSelector?: (outerValue: T, innerValue: I, outerIndex: number, innerIndex: number) => R) {
    super(destination);
  }

  protected _next(value: any) {
    const innerSubscription = this.innerSubscription;
    if (innerSubscription) {
      innerSubscription.unsubscribe();
    }
    this.add(this.innerSubscription = subscribeToResult(this, this.inner, value, this.index++));
  }

  protected _complete() {
    const {innerSubscription} = this;
    if (!innerSubscription || innerSubscription.closed) {
      super._complete();
    }
  }

  /** @deprecated internal use only */ _unsubscribe() {
    this.innerSubscription = null;
  }

  notifyComplete(innerSub: Subscription) {
    this.remove(innerSub);
    this.innerSubscription = null;
    if (this.isStopped) {
      super._complete();
    }
  }

  notifyNext(outerValue: T, innerValue: I,
             outerIndex: number, innerIndex: number,
             innerSub: InnerSubscriber<T, I>): void {
    const { resultSelector, destination } = this;
    if (resultSelector) {
      this.tryResultSelector(outerValue, innerValue, outerIndex, innerIndex);
    } else {
      destination.next(innerValue);
    }
  }

  private tryResultSelector(outerValue: T, innerValue: I,
                            outerIndex: number, innerIndex: number): void {
    const { resultSelector, destination } = this;
    let result: R;
    try {
      result = resultSelector(outerValue, innerValue, outerIndex, innerIndex);
    } catch (err) {
      destination.error(err);
      return;
    }

    destination.next(result);
  }
}
