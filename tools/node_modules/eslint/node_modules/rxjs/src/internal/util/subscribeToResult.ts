import { ObservableInput } from '../types';
import { Subscription } from '../Subscription';
import { InnerSubscriber } from '../InnerSubscriber';
import { OuterSubscriber } from '../OuterSubscriber';
import { Subscriber } from '../Subscriber';
import { subscribeTo } from './subscribeTo';

export function subscribeToResult<T, R>(
  outerSubscriber: OuterSubscriber<T, R>,
  result: any,
  outerValue?: T,
  outerIndex?: number,
  destination?: Subscriber<any>
): Subscription;
export function subscribeToResult<T, R>(
  outerSubscriber: OuterSubscriber<T, R>,
  result: any,
  outerValue?: T,
  outerIndex?: number,
  destination: Subscriber<any> = new InnerSubscriber(outerSubscriber, outerValue, outerIndex)
): Subscription | void {
  if (destination.closed) {
    return;
  }
  return subscribeTo(result)(destination);
}
