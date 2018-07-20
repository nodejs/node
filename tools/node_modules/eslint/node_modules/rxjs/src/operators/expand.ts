import { Observable } from '../Observable';
import { IScheduler } from '../Scheduler';
import { Operator } from '../Operator';
import { Subscriber } from '../Subscriber';
import { tryCatch } from '../util/tryCatch';
import { errorObject } from '../util/errorObject';
import { Subscription } from '../Subscription';
import { OuterSubscriber } from '../OuterSubscriber';
import { InnerSubscriber } from '../InnerSubscriber';
import { subscribeToResult } from '../util/subscribeToResult';
import { MonoTypeOperatorFunction, OperatorFunction } from '../interfaces';

/* tslint:disable:max-line-length */
export function expand<T>(project: (value: T, index: number) => Observable<T>, concurrent?: number, scheduler?: IScheduler): MonoTypeOperatorFunction<T>;
export function expand<T, R>(project: (value: T, index: number) => Observable<R>, concurrent?: number, scheduler?: IScheduler): OperatorFunction<T, R>;
/* tslint:enable:max-line-length */

/**
 * Recursively projects each source value to an Observable which is merged in
 * the output Observable.
 *
 * <span class="informal">It's similar to {@link mergeMap}, but applies the
 * projection function to every source value as well as every output value.
 * It's recursive.</span>
 *
 * <img src="./img/expand.png" width="100%">
 *
 * Returns an Observable that emits items based on applying a function that you
 * supply to each item emitted by the source Observable, where that function
 * returns an Observable, and then merging those resulting Observables and
 * emitting the results of this merger. *Expand* will re-emit on the output
 * Observable every source value. Then, each output value is given to the
 * `project` function which returns an inner Observable to be merged on the
 * output Observable. Those output values resulting from the projection are also
 * given to the `project` function to produce new output values. This is how
 * *expand* behaves recursively.
 *
 * @example <caption>Start emitting the powers of two on every click, at most 10 of them</caption>
 * var clicks = Rx.Observable.fromEvent(document, 'click');
 * var powersOfTwo = clicks
 *   .mapTo(1)
 *   .expand(x => Rx.Observable.of(2 * x).delay(1000))
 *   .take(10);
 * powersOfTwo.subscribe(x => console.log(x));
 *
 * @see {@link mergeMap}
 * @see {@link mergeScan}
 *
 * @param {function(value: T, index: number) => Observable} project A function
 * that, when applied to an item emitted by the source or the output Observable,
 * returns an Observable.
 * @param {number} [concurrent=Number.POSITIVE_INFINITY] Maximum number of input
 * Observables being subscribed to concurrently.
 * @param {Scheduler} [scheduler=null] The IScheduler to use for subscribing to
 * each projected inner Observable.
 * @return {Observable} An Observable that emits the source values and also
 * result of applying the projection function to each value emitted on the
 * output Observable and and merging the results of the Observables obtained
 * from this transformation.
 * @method expand
 * @owner Observable
 */
export function expand<T, R>(project: (value: T, index: number) => Observable<R>,
                             concurrent: number = Number.POSITIVE_INFINITY,
                             scheduler: IScheduler = undefined): OperatorFunction<T, R> {
  concurrent = (concurrent || 0) < 1 ? Number.POSITIVE_INFINITY : concurrent;

  return (source: Observable<T>) => source.lift(new ExpandOperator(project, concurrent, scheduler));
}

export class ExpandOperator<T, R> implements Operator<T, R> {
  constructor(private project: (value: T, index: number) => Observable<R>,
              private concurrent: number,
              private scheduler: IScheduler) {
  }

  call(subscriber: Subscriber<R>, source: any): any {
    return source.subscribe(new ExpandSubscriber(subscriber, this.project, this.concurrent, this.scheduler));
  }
}

interface DispatchArg<T, R> {
  subscriber: ExpandSubscriber<T, R>;
  result: Observable<R>;
  value: any;
  index: number;
}

/**
 * We need this JSDoc comment for affecting ESDoc.
 * @ignore
 * @extends {Ignored}
 */
export class ExpandSubscriber<T, R> extends OuterSubscriber<T, R> {
  private index: number = 0;
  private active: number = 0;
  private hasCompleted: boolean = false;
  private buffer: any[];

  constructor(destination: Subscriber<R>,
              private project: (value: T, index: number) => Observable<R>,
              private concurrent: number,
              private scheduler: IScheduler) {
    super(destination);
    if (concurrent < Number.POSITIVE_INFINITY) {
      this.buffer = [];
    }
  }

  private static dispatch<T, R>(arg: DispatchArg<T, R>): void {
    const {subscriber, result, value, index} = arg;
    subscriber.subscribeToProjection(result, value, index);
  }

  protected _next(value: any): void {
    const destination = this.destination;

    if (destination.closed) {
      this._complete();
      return;
    }

    const index = this.index++;
    if (this.active < this.concurrent) {
      destination.next(value);
      let result = tryCatch(this.project)(value, index);
      if (result === errorObject) {
        destination.error(errorObject.e);
      } else if (!this.scheduler) {
        this.subscribeToProjection(result, value, index);
      } else {
        const state: DispatchArg<T, R> = { subscriber: this, result, value, index };
        this.add(this.scheduler.schedule(ExpandSubscriber.dispatch, 0, state));
      }
    } else {
      this.buffer.push(value);
    }
  }

  private subscribeToProjection(result: any, value: T, index: number): void {
    this.active++;
    this.add(subscribeToResult<T, R>(this, result, value, index));
  }

  protected _complete(): void {
    this.hasCompleted = true;
    if (this.hasCompleted && this.active === 0) {
      this.destination.complete();
    }
  }

  notifyNext(outerValue: T, innerValue: R,
             outerIndex: number, innerIndex: number,
             innerSub: InnerSubscriber<T, R>): void {
    this._next(innerValue);
  }

  notifyComplete(innerSub: Subscription): void {
    const buffer = this.buffer;
    this.remove(innerSub);
    this.active--;
    if (buffer && buffer.length > 0) {
      this._next(buffer.shift());
    }
    if (this.hasCompleted && this.active === 0) {
      this.destination.complete();
    }
  }
}
