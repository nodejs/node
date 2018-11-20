import { Operator } from '../Operator';
import { Subscriber } from '../Subscriber';
import { Observable } from '../Observable';
import { OuterSubscriber } from '../OuterSubscriber';
import { InnerSubscriber } from '../InnerSubscriber';
import { subscribeToResult } from '../util/subscribeToResult';
import { MonoTypeOperatorFunction, TeardownLogic, ObservableInput } from '../types';
import { Subscription } from '../Subscription';

/**
 * Returns an Observable that skips items emitted by the source Observable until a second Observable emits an item.
 *
 * ![](skipUntil.png)
 *
 * @param {Observable} notifier - The second Observable that has to emit an item before the source Observable's elements begin to
 * be mirrored by the resulting Observable.
 * @return {Observable<T>} An Observable that skips items from the source Observable until the second Observable emits
 * an item, then emits the remaining items.
 * @method skipUntil
 * @owner Observable
 */
export function skipUntil<T>(notifier: Observable<any>): MonoTypeOperatorFunction<T> {
  return (source: Observable<T>) => source.lift(new SkipUntilOperator(notifier));
}

class SkipUntilOperator<T> implements Operator<T, T> {
  constructor(private notifier: Observable<any>) {
  }

  call(destination: Subscriber<T>, source: any): TeardownLogic {
    return source.subscribe(new SkipUntilSubscriber(destination, this.notifier));
  }
}

/**
 * We need this JSDoc comment for affecting ESDoc.
 * @ignore
 * @extends {Ignored}
 */
class SkipUntilSubscriber<T, R> extends OuterSubscriber<T, R> {

  private hasValue: boolean = false;
  private innerSubscription: Subscription;

  constructor(destination: Subscriber<R>, notifier: ObservableInput<any>) {
    super(destination);
    const innerSubscriber = new InnerSubscriber(this, undefined, undefined);
    this.add(innerSubscriber);
    this.innerSubscription = innerSubscriber;
    subscribeToResult(this, notifier, undefined, undefined, innerSubscriber);
  }

  protected _next(value: T) {
    if (this.hasValue) {
      super._next(value);
    }
  }

  notifyNext(outerValue: T, innerValue: R,
             outerIndex: number, innerIndex: number,
             innerSub: InnerSubscriber<T, R>): void {
    this.hasValue = true;
    if (this.innerSubscription) {
      this.innerSubscription.unsubscribe();
    }
  }

  notifyComplete() {
    /* do nothing */
  }
}
