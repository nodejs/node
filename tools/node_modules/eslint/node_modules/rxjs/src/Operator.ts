import { Subscriber } from './Subscriber';
import { TeardownLogic } from './Subscription';

export interface Operator<T, R> {
  call(subscriber: Subscriber<R>, source: any): TeardownLogic;
}
