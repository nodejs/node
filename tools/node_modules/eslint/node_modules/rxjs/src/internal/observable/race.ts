import { Observable } from '../Observable';
import { isArray } from '../util/isArray';
import { fromArray } from './fromArray';
import { Operator } from '../Operator';
import { Subscriber } from '../Subscriber';
import { Subscription } from '../Subscription';
import { TeardownLogic } from '../types';
import { OuterSubscriber } from '../OuterSubscriber';
import { InnerSubscriber } from '../InnerSubscriber';
import { subscribeToResult } from '../util/subscribeToResult';

// tslint:disable:max-line-length
export function race<A, B>(a: Observable<A>, b: Observable<B>): Observable<A> | Observable<B>;
export function race<A, B, C>(a: Observable<A>, b: Observable<B>, c: Observable<C>): Observable<A> | Observable<B> | Observable<C>;
export function race<A, B, C, D>(a: Observable<A>, b: Observable<B>, c: Observable<C>, d: Observable<D>): Observable<A> | Observable<B> | Observable<C> | Observable<D>;
export function race<A, B, C, D, E>(a: Observable<A>, b: Observable<B>, c: Observable<C>, d: Observable<D>, e: Observable<E>): Observable<A> | Observable<B> | Observable<C> | Observable<D> | Observable<E>;
// tslint:enable:max-line-length

export function race<T>(observables: Observable<T>[]): Observable<T>;
export function race(observables: Observable<any>[]): Observable<{}>;
export function race<T>(...observables: Observable<T>[]): Observable<T>;
export function race(...observables: Observable<any>[]): Observable<{}>;

/**
 * Returns an Observable that mirrors the first source Observable to emit an item.
 *
 * ## Example
 * ### Subscribes to the observable that was the first to start emitting.
 *
 * ```javascript
 * import { race, interval } from 'rxjs';
 * import { mapTo } from 'rxjs/operators';
 *
 * const obs1 = interval(1000).pipe(mapTo('fast one'));
 * const obs2 = interval(3000).pipe(mapTo('medium one'));
 * const obs3 = interval(5000).pipe(mapTo('slow one'));
 *
 * race(obs3, obs1, obs2)
 * .subscribe(
 *   winner => console.log(winner)
 * );
 *
 * // result:
 * // a series of 'fast one'
 * ```
 *
 * @param {...Observables} ...observables sources used to race for which Observable emits first.
 * @return {Observable} an Observable that mirrors the output of the first Observable to emit an item.
 * @static true
 * @name race
 * @owner Observable
 */
export function race<T>(...observables: (Observable<any>[] | Observable<any>)[]): Observable<T> {
  // if the only argument is an array, it was most likely called with
  // `race([obs1, obs2, ...])`
  if (observables.length === 1) {
    if (isArray(observables[0])) {
      observables = observables[0] as Observable<any>[];
    } else {
      return observables[0] as Observable<T>;
    }
  }

  return fromArray(observables, undefined).lift(new RaceOperator<T>());
}

export class RaceOperator<T> implements Operator<T, T> {
  call(subscriber: Subscriber<T>, source: any): TeardownLogic {
    return source.subscribe(new RaceSubscriber(subscriber));
  }
}

/**
 * We need this JSDoc comment for affecting ESDoc.
 * @ignore
 * @extends {Ignored}
 */
export class RaceSubscriber<T> extends OuterSubscriber<T, T> {
  private hasFirst: boolean = false;
  private observables: Observable<any>[] = [];
  private subscriptions: Subscription[] = [];

  constructor(destination: Subscriber<T>) {
    super(destination);
  }

  protected _next(observable: any): void {
    this.observables.push(observable);
  }

  protected _complete() {
    const observables = this.observables;
    const len = observables.length;

    if (len === 0) {
      this.destination.complete();
    } else {
      for (let i = 0; i < len && !this.hasFirst; i++) {
        let observable = observables[i];
        let subscription = subscribeToResult(this, observable, observable as any, i);

        if (this.subscriptions) {
          this.subscriptions.push(subscription);
        }
        this.add(subscription);
      }
      this.observables = null;
    }
  }

  notifyNext(outerValue: T, innerValue: T,
             outerIndex: number, innerIndex: number,
             innerSub: InnerSubscriber<T, T>): void {
    if (!this.hasFirst) {
      this.hasFirst = true;

      for (let i = 0; i < this.subscriptions.length; i++) {
        if (i !== outerIndex) {
          let subscription = this.subscriptions[i];

          subscription.unsubscribe();
          this.remove(subscription);
        }
      }

      this.subscriptions = null;
    }

    this.destination.next(innerValue);
  }
}
