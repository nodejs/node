import { Operator } from '../Operator';
import { Observable, ObservableInput } from '../Observable';
import { Subscriber } from '../Subscriber';
import { Subscription } from '../Subscription';
import { OuterSubscriber } from '../OuterSubscriber';
import { InnerSubscriber } from '../InnerSubscriber';
import { subscribeToResult } from '../util/subscribeToResult';
import { OperatorFunction } from '../interfaces';

/* tslint:disable:max-line-length */
export function exhaustMap<T, R>(project: (value: T, index: number) => ObservableInput<R>): OperatorFunction<T, R>;
export function exhaustMap<T, I, R>(project: (value: T, index: number) => ObservableInput<I>, resultSelector: (outerValue: T, innerValue: I, outerIndex: number, innerIndex: number) => R): OperatorFunction<T, R>;
/* tslint:enable:max-line-length */

/**
 * Projects each source value to an Observable which is merged in the output
 * Observable only if the previous projected Observable has completed.
 *
 * <span class="informal">Maps each value to an Observable, then flattens all of
 * these inner Observables using {@link exhaust}.</span>
 *
 * <img src="./img/exhaustMap.png" width="100%">
 *
 * Returns an Observable that emits items based on applying a function that you
 * supply to each item emitted by the source Observable, where that function
 * returns an (so-called "inner") Observable. When it projects a source value to
 * an Observable, the output Observable begins emitting the items emitted by
 * that projected Observable. However, `exhaustMap` ignores every new projected
 * Observable if the previous projected Observable has not yet completed. Once
 * that one completes, it will accept and flatten the next projected Observable
 * and repeat this process.
 *
 * @example <caption>Run a finite timer for each click, only if there is no currently active timer</caption>
 * var clicks = Rx.Observable.fromEvent(document, 'click');
 * var result = clicks.exhaustMap((ev) => Rx.Observable.interval(1000).take(5));
 * result.subscribe(x => console.log(x));
 *
 * @see {@link concatMap}
 * @see {@link exhaust}
 * @see {@link mergeMap}
 * @see {@link switchMap}
 *
 * @param {function(value: T, ?index: number): ObservableInput} project A function
 * that, when applied to an item emitted by the source Observable, returns an
 * Observable.
 * @param {function(outerValue: T, innerValue: I, outerIndex: number, innerIndex: number): any} [resultSelector]
 * A function to produce the value on the output Observable based on the values
 * and the indices of the source (outer) emission and the inner Observable
 * emission. The arguments passed to this function are:
 * - `outerValue`: the value that came from the source
 * - `innerValue`: the value that came from the projected Observable
 * - `outerIndex`: the "index" of the value that came from the source
 * - `innerIndex`: the "index" of the value from the projected Observable
 * @return {Observable} An Observable containing projected Observables
 * of each item of the source, ignoring projected Observables that start before
 * their preceding Observable has completed.
 * @method exhaustMap
 * @owner Observable
 */
export function exhaustMap<T, I, R>(
  project: (value: T, index: number) => ObservableInput<I>,
  resultSelector?: (outerValue: T, innerValue: I, outerIndex: number, innerIndex: number) => R
  ): OperatorFunction<T, R> {
    return (source: Observable<T>) => source.lift(new SwitchFirstMapOperator(project, resultSelector));
  }

class SwitchFirstMapOperator<T, I, R> implements Operator<T, R> {
  constructor(private project: (value: T, index: number) => ObservableInput<I>,
              private resultSelector?: (outerValue: T, innerValue: I, outerIndex: number, innerIndex: number) => R) {
  }

  call(subscriber: Subscriber<R>, source: any): any {
    return source.subscribe(new SwitchFirstMapSubscriber(subscriber, this.project, this.resultSelector));
  }
}

/**
 * We need this JSDoc comment for affecting ESDoc.
 * @ignore
 * @extends {Ignored}
 */
class SwitchFirstMapSubscriber<T, I, R> extends OuterSubscriber<T, I> {
  private hasSubscription: boolean = false;
  private hasCompleted: boolean = false;
  private index: number = 0;

  constructor(destination: Subscriber<R>,
              private project: (value: T, index: number) => ObservableInput<I>,
              private resultSelector?: (outerValue: T, innerValue: I, outerIndex: number, innerIndex: number) => R) {
    super(destination);
  }

  protected _next(value: T): void {
    if (!this.hasSubscription) {
      this.tryNext(value);
    }
  }

  private tryNext(value: T): void {
    const index = this.index++;
    const destination = this.destination;
    try {
      const result = this.project(value, index);
      this.hasSubscription = true;
      this.add(subscribeToResult(this, result, value, index));
    } catch (err) {
      destination.error(err);
    }
  }

  protected _complete(): void {
    this.hasCompleted = true;
    if (!this.hasSubscription) {
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
    try {
      const result = resultSelector(outerValue, innerValue, outerIndex, innerIndex);
      destination.next(result);
    } catch (err) {
      destination.error(err);
    }
  }

  notifyError(err: any): void {
    this.destination.error(err);
  }

  notifyComplete(innerSub: Subscription): void {
    this.remove(innerSub);

    this.hasSubscription = false;
    if (this.hasCompleted) {
      this.destination.complete();
    }
  }
}
