import { Observable } from '../Observable';
import { Operator } from '../Operator';
import { Subscriber } from '../Subscriber';
import { noop } from '../util/noop';
import { MonoTypeOperatorFunction } from '../interfaces';

/**
 * Ignores all items emitted by the source Observable and only passes calls of `complete` or `error`.
 *
 * <img src="./img/ignoreElements.png" width="100%">
 *
 * @return {Observable} An empty Observable that only calls `complete`
 * or `error`, based on which one is called by the source Observable.
 * @method ignoreElements
 * @owner Observable
 */
export function ignoreElements<T>(): MonoTypeOperatorFunction<T> {
  return function ignoreElementsOperatorFunction(source: Observable<T>) {
    return source.lift(new IgnoreElementsOperator());
  };
}

class IgnoreElementsOperator<T, R> implements Operator<T, R> {
  call(subscriber: Subscriber<R>, source: any): any {
    return source.subscribe(new IgnoreElementsSubscriber(subscriber));
  }
}

/**
 * We need this JSDoc comment for affecting ESDoc.
 * @ignore
 * @extends {Ignored}
 */
class IgnoreElementsSubscriber<T> extends Subscriber<T> {
  protected _next(unused: T): void {
    noop();
  }
}
