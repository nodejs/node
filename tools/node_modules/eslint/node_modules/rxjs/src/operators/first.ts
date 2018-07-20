import { Observable } from '../Observable';
import { Operator } from '../Operator';
import { Subscriber } from '../Subscriber';
import { EmptyError } from '../util/EmptyError';
import { OperatorFunction, MonoTypeOperatorFunction } from '../interfaces';
/* tslint:disable:max-line-length */
export function first<T, S extends T>(predicate: (value: T, index: number, source: Observable<T>) => value is S): OperatorFunction<T, S>;
export function first<T, S extends T, R>(predicate: (value: T | S, index: number, source: Observable<T>) => value is S,
                                         resultSelector: (value: S, index: number) => R, defaultValue?: R): OperatorFunction<T, R>;
export function first<T, S extends T>(predicate: (value: T, index: number, source: Observable<T>) => value is S,
                                      resultSelector: void,
                                      defaultValue?: S): OperatorFunction<T, S>;
export function first<T>(predicate?: (value: T, index: number, source: Observable<T>) => boolean): MonoTypeOperatorFunction<T>;
export function first<T, R>(predicate: (value: T, index: number, source: Observable<T>) => boolean,
                            resultSelector?: (value: T, index: number) => R,
                            defaultValue?: R): OperatorFunction<T, R>;
export function first<T>(predicate: (value: T, index: number, source: Observable<T>) => boolean,
                         resultSelector: void,
                         defaultValue?: T): MonoTypeOperatorFunction<T>;

/**
 * Emits only the first value (or the first value that meets some condition)
 * emitted by the source Observable.
 *
 * <span class="informal">Emits only the first value. Or emits only the first
 * value that passes some test.</span>
 *
 * <img src="./img/first.png" width="100%">
 *
 * If called with no arguments, `first` emits the first value of the source
 * Observable, then completes. If called with a `predicate` function, `first`
 * emits the first value of the source that matches the specified condition. It
 * may also take a `resultSelector` function to produce the output value from
 * the input value, and a `defaultValue` to emit in case the source completes
 * before it is able to emit a valid value. Throws an error if `defaultValue`
 * was not provided and a matching element is not found.
 *
 * @example <caption>Emit only the first click that happens on the DOM</caption>
 * var clicks = Rx.Observable.fromEvent(document, 'click');
 * var result = clicks.first();
 * result.subscribe(x => console.log(x));
 *
 * @example <caption>Emits the first click that happens on a DIV</caption>
 * var clicks = Rx.Observable.fromEvent(document, 'click');
 * var result = clicks.first(ev => ev.target.tagName === 'DIV');
 * result.subscribe(x => console.log(x));
 *
 * @see {@link filter}
 * @see {@link find}
 * @see {@link take}
 *
 * @throws {EmptyError} Delivers an EmptyError to the Observer's `error`
 * callback if the Observable completes before any `next` notification was sent.
 *
 * @param {function(value: T, index: number, source: Observable<T>): boolean} [predicate]
 * An optional function called with each item to test for condition matching.
 * @param {function(value: T, index: number): R} [resultSelector] A function to
 * produce the value on the output Observable based on the values
 * and the indices of the source Observable. The arguments passed to this
 * function are:
 * - `value`: the value that was emitted on the source.
 * - `index`: the "index" of the value from the source.
 * @param {R} [defaultValue] The default value emitted in case no valid value
 * was found on the source.
 * @return {Observable<T|R>} An Observable of the first item that matches the
 * condition.
 * @method first
 * @owner Observable
 */
export function first<T, R>(predicate?: (value: T, index: number, source: Observable<T>) => boolean,
                            resultSelector?: ((value: T, index: number) => R) | void,
                            defaultValue?: R): OperatorFunction<T, T | R> {
  return (source: Observable<T>) => source.lift(new FirstOperator(predicate, resultSelector, defaultValue, source));
}

class FirstOperator<T, R> implements Operator<T, R> {
  constructor(private predicate?: (value: T, index: number, source: Observable<T>) => boolean,
              private resultSelector?: ((value: T, index: number) => R) | void,
              private defaultValue?: any,
              private source?: Observable<T>) {
  }

  call(observer: Subscriber<R>, source: any): any {
    return source.subscribe(new FirstSubscriber(observer, this.predicate, this.resultSelector, this.defaultValue, this.source));
  }
}

/**
 * We need this JSDoc comment for affecting ESDoc.
 * @ignore
 * @extends {Ignored}
 */
class FirstSubscriber<T, R> extends Subscriber<T> {
  private index: number = 0;
  private hasCompleted: boolean = false;
  private _emitted: boolean = false;

  constructor(destination: Subscriber<R>,
              private predicate?: (value: T, index: number, source: Observable<T>) => boolean,
              private resultSelector?: ((value: T, index: number) => R) | void,
              private defaultValue?: any,
              private source?: Observable<T>) {
    super(destination);
  }

  protected _next(value: T): void {
    const index = this.index++;
    if (this.predicate) {
      this._tryPredicate(value, index);
    } else {
      this._emit(value, index);
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
      this._emit(value, index);
    }
  }

  private _emit(value: any, index: number) {
    if (this.resultSelector) {
      this._tryResultSelector(value, index);
      return;
    }
    this._emitFinal(value);
  }

  private _tryResultSelector(value: T, index: number) {
    let result: any;
    try {
      result = (<any>this).resultSelector(value, index);
    } catch (err) {
      this.destination.error(err);
      return;
    }
    this._emitFinal(result);
  }

  private _emitFinal(value: any) {
    const destination = this.destination;
    if (!this._emitted) {
      this._emitted = true;
      destination.next(value);
      destination.complete();
      this.hasCompleted = true;
    }
  }

  protected _complete(): void {
    const destination = this.destination;
    if (!this.hasCompleted && typeof this.defaultValue !== 'undefined') {
      destination.next(this.defaultValue);
      destination.complete();
    } else if (!this.hasCompleted) {
      destination.error(new EmptyError);
    }
  }
}
