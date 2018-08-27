import { Observable } from '../Observable';
import { Operator } from '../Operator';
import { Subscriber } from '../Subscriber';
import { EmptyError } from '../util/EmptyError';
import { OperatorFunction, MonoTypeOperatorFunction } from '../interfaces';

/* tslint:disable:max-line-length */
export function last<T, S extends T>(predicate: (value: T, index: number, source: Observable<T>) => value is S): OperatorFunction<T, S>;
export function last<T, S extends T, R>(predicate: (value: T | S, index: number, source: Observable<T>) => value is S,
                                        resultSelector: (value: S, index: number) => R, defaultValue?: R): OperatorFunction<T, R>;
export function last<T, S extends T>(predicate: (value: T, index: number, source: Observable<T>) => value is S,
                                     resultSelector: void,
                                     defaultValue?: S): OperatorFunction<T, S>;
export function last<T>(predicate?: (value: T, index: number, source: Observable<T>) => boolean): MonoTypeOperatorFunction<T>;
export function last<T, R>(predicate: (value: T, index: number, source: Observable<T>) => boolean,
                           resultSelector?: (value: T, index: number) => R,
                           defaultValue?: R): OperatorFunction<T, R>;
export function last<T>(predicate: (value: T, index: number, source: Observable<T>) => boolean,
                        resultSelector: void,
                        defaultValue?: T): MonoTypeOperatorFunction<T>;
/* tslint:enable:max-line-length */

/**
 * Returns an Observable that emits only the last item emitted by the source Observable.
 * It optionally takes a predicate function as a parameter, in which case, rather than emitting
 * the last item from the source Observable, the resulting Observable will emit the last item
 * from the source Observable that satisfies the predicate.
 *
 * <img src="./img/last.png" width="100%">
 *
 * @throws {EmptyError} Delivers an EmptyError to the Observer's `error`
 * callback if the Observable completes before any `next` notification was sent.
 * @param {function} predicate - The condition any source emitted item has to satisfy.
 * @return {Observable} An Observable that emits only the last item satisfying the given condition
 * from the source, or an NoSuchElementException if no such items are emitted.
 * @throws - Throws if no items that match the predicate are emitted by the source Observable.
 * @method last
 * @owner Observable
 */
export function last<T, R>(predicate?: (value: T, index: number, source: Observable<T>) => boolean,
                           resultSelector?: ((value: T, index: number) => R) | void,
                           defaultValue?: R): OperatorFunction<T, T | R> {
  return (source: Observable<T>) => source.lift(new LastOperator(predicate, resultSelector, defaultValue, source));
}

class LastOperator<T, R> implements Operator<T, R> {
  constructor(private predicate?: (value: T, index: number, source: Observable<T>) => boolean,
              private resultSelector?: ((value: T, index: number) => R) | void,
              private defaultValue?: any,
              private source?: Observable<T>) {
  }

  call(observer: Subscriber<R>, source: any): any {
    return source.subscribe(new LastSubscriber(observer, this.predicate, this.resultSelector, this.defaultValue, this.source));
  }
}

/**
 * We need this JSDoc comment for affecting ESDoc.
 * @ignore
 * @extends {Ignored}
 */
class LastSubscriber<T, R> extends Subscriber<T> {
  private lastValue: T | R;
  private hasValue: boolean = false;
  private index: number = 0;

  constructor(destination: Subscriber<R>,
              private predicate?: (value: T, index: number, source: Observable<T>) => boolean,
              private resultSelector?: ((value: T, index: number) => R) | void,
              private defaultValue?: any,
              private source?: Observable<T>) {
    super(destination);
    if (typeof defaultValue !== 'undefined') {
      this.lastValue = defaultValue;
      this.hasValue = true;
    }
  }

  protected _next(value: T): void {
    const index = this.index++;
    if (this.predicate) {
      this._tryPredicate(value, index);
    } else {
      if (this.resultSelector) {
        this._tryResultSelector(value, index);
        return;
      }
      this.lastValue = value;
      this.hasValue = true;
    }
  }

  private _tryPredicate(value: T, index: number) {
    let result: any;
    try {
      result = this.predicate(value, index, this.source);
    } catch (err) {
      this.destination.error(err);
      return;
    }
    if (result) {
      if (this.resultSelector) {
        this._tryResultSelector(value, index);
        return;
      }
      this.lastValue = value;
      this.hasValue = true;
    }
  }

  private _tryResultSelector(value: T, index: number) {
    let result: any;
    try {
      result = (<any>this).resultSelector(value, index);
    } catch (err) {
      this.destination.error(err);
      return;
    }
    this.lastValue = result;
    this.hasValue = true;
  }

  protected _complete(): void {
    const destination = this.destination;
    if (this.hasValue) {
      destination.next(this.lastValue);
      destination.complete();
    } else {
      destination.error(new EmptyError);
    }
  }
}
