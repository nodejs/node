import { Observable } from '../Observable';
import { ReplaySubject } from '../ReplaySubject';
import { Subscription } from '../Subscription';
import { MonoTypeOperatorFunction, SchedulerLike } from '../types';
import { Subscriber } from '../Subscriber';

/**
 * Share source and replay specified number of emissions on subscription.
 *
 * This operator is a specialization of `replay` that connects to a source observable
 * and multicasts through a `ReplaySubject` constructed with the specified arguments.
 * A successfully completed source will stay cached in the `shareReplayed observable` forever,
 * but an errored source can be retried.
 *
 * ## Why use shareReplay?
 * You generally want to use `shareReplay` when you have side-effects or taxing computations
 * that you do not wish to be executed amongst multiple subscribers.
 * It may also be valuable in situations where you know you will have late subscribers to
 * a stream that need access to previously emitted values.
 * This ability to replay values on subscription is what differentiates {@link share} and `shareReplay`.
 *
 * ![](shareReplay.png)
 *
 * ## Example
 * ```javascript
 * const obs$ = interval(1000);
 * const subscription = obs$.pipe(
 *   take(4),
 *   shareReplay(3)
 * );
 * subscription.subscribe(x => console.log('source A: ', x));
 * subscription.subscribe(y => console.log('source B: ', y));
 *
 * ```
 *
 * @see {@link publish}
 * @see {@link share}
 * @see {@link publishReplay}
 *
 * @param {Number} [bufferSize=Number.POSITIVE_INFINITY] Maximum element count of the replay buffer.
 * @param {Number} [windowTime=Number.MAX_VALUE] Maximum time length of the replay buffer in milliseconds.
 * @param {Scheduler} [scheduler] Scheduler where connected observers within the selector function
 * will be invoked on.
 * @return {Observable} An observable sequence that contains the elements of a sequence produced
 * by multicasting the source sequence within a selector function.
 * @method shareReplay
 * @owner Observable
 */
export function shareReplay<T>(
  bufferSize: number = Number.POSITIVE_INFINITY,
  windowTime: number = Number.MAX_VALUE,
  scheduler?: SchedulerLike
): MonoTypeOperatorFunction<T> {
  return (source: Observable<T>) => source.lift(shareReplayOperator(bufferSize, windowTime, scheduler));
}

function shareReplayOperator<T>(bufferSize?: number, windowTime?: number, scheduler?: SchedulerLike) {
  let subject: ReplaySubject<T>;
  let refCount = 0;
  let subscription: Subscription;
  let hasError = false;
  let isComplete = false;

  return function shareReplayOperation(this: Subscriber<T>, source: Observable<T>) {
    refCount++;
    if (!subject || hasError) {
      hasError = false;
      subject = new ReplaySubject<T>(bufferSize, windowTime, scheduler);
      subscription = source.subscribe({
        next(value) { subject.next(value); },
        error(err) {
          hasError = true;
          subject.error(err);
        },
        complete() {
          isComplete = true;
          subject.complete();
        },
      });
    }

    const innerSub = subject.subscribe(this);

    return () => {
      refCount--;
      innerSub.unsubscribe();
      if (subscription && refCount === 0 && isComplete) {
        subscription.unsubscribe();
      }
    };
  };
}
