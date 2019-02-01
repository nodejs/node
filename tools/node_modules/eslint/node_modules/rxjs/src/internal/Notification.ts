import { PartialObserver } from './types';
import { Observable } from './Observable';
import { empty } from './observable/empty';
import { of } from './observable/of';
import { throwError } from './observable/throwError';

export const enum NotificationKind {
  NEXT = 'N',
  ERROR = 'E',
  COMPLETE = 'C',
}

/**
 * Represents a push-based event or value that an {@link Observable} can emit.
 * This class is particularly useful for operators that manage notifications,
 * like {@link materialize}, {@link dematerialize}, {@link observeOn}, and
 * others. Besides wrapping the actual delivered value, it also annotates it
 * with metadata of, for instance, what type of push message it is (`next`,
 * `error`, or `complete`).
 *
 * @see {@link materialize}
 * @see {@link dematerialize}
 * @see {@link observeOn}
 *
 * @class Notification<T>
 */
export class Notification<T> {
  hasValue: boolean;

  constructor(public kind: NotificationKind, public value?: T, public error?: any) {
    this.hasValue = kind === NotificationKind.NEXT;
  }

  /**
   * Delivers to the given `observer` the value wrapped by this Notification.
   * @param {Observer} observer
   * @return
   */
  observe(observer: PartialObserver<T>): any {
    switch (this.kind) {
      case NotificationKind.NEXT:
        return observer.next && observer.next(this.value);
      case NotificationKind.ERROR:
        return observer.error && observer.error(this.error);
      case NotificationKind.COMPLETE:
        return observer.complete && observer.complete();
    }
  }

  /**
   * Given some {@link Observer} callbacks, deliver the value represented by the
   * current Notification to the correctly corresponding callback.
   * @param {function(value: T): void} next An Observer `next` callback.
   * @param {function(err: any): void} [error] An Observer `error` callback.
   * @param {function(): void} [complete] An Observer `complete` callback.
   * @return {any}
   */
  do(next: (value: T) => void, error?: (err: any) => void, complete?: () => void): any {
    const kind = this.kind;
    switch (kind) {
      case NotificationKind.NEXT:
        return next && next(this.value);
      case NotificationKind.ERROR:
        return error && error(this.error);
      case NotificationKind.COMPLETE:
        return complete && complete();
    }
  }

  /**
   * Takes an Observer or its individual callback functions, and calls `observe`
   * or `do` methods accordingly.
   * @param {Observer|function(value: T): void} nextOrObserver An Observer or
   * the `next` callback.
   * @param {function(err: any): void} [error] An Observer `error` callback.
   * @param {function(): void} [complete] An Observer `complete` callback.
   * @return {any}
   */
  accept(nextOrObserver: PartialObserver<T> | ((value: T) => void), error?: (err: any) => void, complete?: () => void) {
    if (nextOrObserver && typeof (<PartialObserver<T>>nextOrObserver).next === 'function') {
      return this.observe(<PartialObserver<T>>nextOrObserver);
    } else {
      return this.do(<(value: T) => void>nextOrObserver, error, complete);
    }
  }

  /**
   * Returns a simple Observable that just delivers the notification represented
   * by this Notification instance.
   * @return {any}
   */
  toObservable(): Observable<T> {
    const kind = this.kind;
    switch (kind) {
      case NotificationKind.NEXT:
        return of(this.value);
      case NotificationKind.ERROR:
        return throwError(this.error);
      case NotificationKind.COMPLETE:
        return empty();
    }
    throw new Error('unexpected notification kind value');
  }

  private static completeNotification: Notification<any> = new Notification(NotificationKind.COMPLETE);
  private static undefinedValueNotification: Notification<any> = new Notification(NotificationKind.NEXT, undefined);

  /**
   * A shortcut to create a Notification instance of the type `next` from a
   * given value.
   * @param {T} value The `next` value.
   * @return {Notification<T>} The "next" Notification representing the
   * argument.
   * @nocollapse
   */
  static createNext<T>(value: T): Notification<T> {
    if (typeof value !== 'undefined') {
      return new Notification(NotificationKind.NEXT, value);
    }
    return Notification.undefinedValueNotification;
  }

  /**
   * A shortcut to create a Notification instance of the type `error` from a
   * given error.
   * @param {any} [err] The `error` error.
   * @return {Notification<T>} The "error" Notification representing the
   * argument.
   * @nocollapse
   */
  static createError<T>(err?: any): Notification<T> {
    return new Notification(NotificationKind.ERROR, undefined, err);
  }

  /**
   * A shortcut to create a Notification instance of the type `complete`.
   * @return {Notification<any>} The valueless "complete" Notification.
   * @nocollapse
   */
  static createComplete(): Notification<any> {
    return Notification.completeNotification;
  }
}
