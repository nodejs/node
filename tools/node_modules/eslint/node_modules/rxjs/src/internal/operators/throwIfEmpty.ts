import { tap } from './tap';
import { EmptyError } from '../util/EmptyError';
import { MonoTypeOperatorFunction } from '../types';

/**
 * If the source observable completes without emitting a value, it will emit
 * an error. The error will be created at that time by the optional
 * `errorFactory` argument, otherwise, the error will be {@link EmptyError}.
 *
 * ![](throwIfEmpty.png)
 *
 * ## Example
 * ```javascript
 * const click$ = fromEvent(button, 'click');
 *
 * clicks$.pipe(
 *   takeUntil(timer(1000)),
 *   throwIfEmpty(
 *     () => new Error('the button was not clicked within 1 second')
 *   ),
 * )
 * .subscribe({
 *   next() { console.log('The button was clicked'); },
 *   error(err) { console.error(err); },
 * });
 * ```
 *
 * @param {Function} [errorFactory] A factory function called to produce the
 * error to be thrown when the source observable completes without emitting a
 * value.
 */
export const throwIfEmpty =
  <T>(errorFactory: (() => any) = defaultErrorFactory) => tap<T>({
    hasValue: false,
    next() { this.hasValue = true; },
    complete() {
      if (!this.hasValue) {
        throw errorFactory();
      }
    }
  } as any);

function defaultErrorFactory() {
  return new EmptyError();
}
