import { Observable, ObservableInput } from '../Observable';
import { Operator } from '../Operator';
import { PartialObserver } from '../Observer';
import { Subscriber } from '../Subscriber';
import { Subscription } from '../Subscription';
import { OuterSubscriber } from '../OuterSubscriber';
import { InnerSubscriber } from '../InnerSubscriber';
import { subscribeToResult } from '../util/subscribeToResult';
import { OperatorFunction } from '../interfaces';

/* tslint:disable:max-line-length */
export function mergeMapTo<T, R>(observable: ObservableInput<R>, concurrent?: number): OperatorFunction<T, R>;
export function mergeMapTo<T, I, R>(observable: ObservableInput<I>, resultSelector: (outerValue: T, innerValue: I, outerIndex: number, innerIndex: number) => R, concurrent?: number): OperatorFunction<T, R>;
/* tslint:enable:max-line-length */

/**
 * Projects each source value to the same Observable which is merged multiple
 * times in the output Observable.
 *
 * <span class="informal">It's like {@link mergeMap}, but maps each value always
 * to the same inner Observable.</span>
 *
 * <img src="./img/mergeMapTo.png" width="100%">
 *
 * Maps each source value to the given Observable `innerObservable` regardless
 * of the source value, and then merges those resulting Observables into one
 * single Observable, which is the output Observable.
 *
 * @example <caption>For each click event, start an interval Observable ticking every 1 second</caption>
 * var clicks = Rx.Observable.fromEvent(document, 'click');
 * var result = clicks.mergeMapTo(Rx.Observable.interval(1000));
 * result.subscribe(x => console.log(x));
 *
 * @see {@link concatMapTo}
 * @see {@link merge}
 * @see {@link mergeAll}
 * @see {@link mergeMap}
 * @see {@link mergeScan}
 * @see {@link switchMapTo}
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
 * @param {number} [concurrent=Number.POSITIVE_INFINITY] Maximum number of input
 * Observables being subscribed to concurrently.
 * @return {Observable} An Observable that emits items from the given
 * `innerObservable` (and optionally transformed through `resultSelector`) every
 * time a value is emitted on the source Observable.
 * @method mergeMapTo
 * @owner Observable
 */
export function mergeMapTo<T, I, R>(innerObservable: Observable<I>,
                                    resultSelector?: ((outerValue: T, innerValue: I, outerIndex: number, innerIndex: number) => R) | number,
                                    concurrent: number = Number.POSITIVE_INFINITY): OperatorFunction<T, R> {
  if (typeof resultSelector === 'number') {
    concurrent = <number>resultSelector;
    resultSelector = null;
  }
  return (source: Observable<T>) => source.lift(new MergeMapToOperator(innerObservable, <any>resultSelector, concurrent));
}

// TODO: Figure out correct signature here: an Operator<Observable<T>, R>
//       needs to implement call(observer: Subscriber<R>): Subscriber<Observable<T>>
export class MergeMapToOperator<T, I, R> implements Operator<Observable<T>, R> {
  constructor(private ish: ObservableInput<I>,
              private resultSelector?: (outerValue: T, innerValue: I, outerIndex: number, innerIndex: number) => R,
              private concurrent: number = Number.POSITIVE_INFINITY) {
  }

  call(observer: Subscriber<R>, source: any): any {
    return source.subscribe(new MergeMapToSubscriber(observer, this.ish, this.resultSelector, this.concurrent));
  }
}

/**
 * We need this JSDoc comment for affecting ESDoc.
 * @ignore
 * @extends {Ignored}
 */
export class MergeMapToSubscriber<T, I, R> extends OuterSubscriber<T, I> {
  private hasCompleted: boolean = false;
  private buffer: T[] = [];
  private active: number = 0;
  protected index: number = 0;

  constructor(destination: Subscriber<R>,
              private ish: ObservableInput<I>,
              private resultSelector?: (outerValue: T, innerValue: I, outerIndex: number, innerIndex: number) => R,
              private concurrent: number = Number.POSITIVE_INFINITY) {
    super(destination);
  }

  protected _next(value: T): void {
    if (this.active < this.concurrent) {
      const resultSelector = this.resultSelector;
      const index = this.index++;
      const ish = this.ish;
      const destination = this.destination;

      this.active++;
      this._innerSub(ish, destination, resultSelector, value, index);
    } else {
      this.buffer.push(value);
    }
  }

  private _innerSub(ish: ObservableInput<I>,
                    destination: PartialObserver<I>,
                    resultSelector: (outerValue: T, innerValue: I, outerIndex: number, innerIndex: number) => R,
                    value: T,
                    index: number): void {
    this.add(subscribeToResult<T, I>(this, ish, value, index));
  }

  protected _complete(): void {
    this.hasCompleted = true;
    if (this.active === 0 && this.buffer.length === 0) {
      this.destination.complete();
    }
  }

  notifyNext(outerValue: T, innerValue: I,
             outerIndex: number, innerIndex: number,
             innerSub: InnerSubscriber<T, I>): void {
    const { resultSelector, destination } = this;
    if (resultSelector) {
      this.trySelectResult(outerValue, innerValue, outerIndex, innerIndex);
    } else {
      destination.next(innerValue);
    }
  }

  private trySelectResult(outerValue: T, innerValue: I,
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

  notifyError(err: any): void {
    this.destination.error(err);
  }

  notifyComplete(innerSub: Subscription): void {
    const buffer = this.buffer;
    this.remove(innerSub);
    this.active--;
    if (buffer.length > 0) {
      this._next(buffer.shift());
    } else if (this.active === 0 && this.hasCompleted) {
      this.destination.complete();
    }
  }
}
