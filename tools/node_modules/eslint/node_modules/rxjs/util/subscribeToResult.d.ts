import { Subscription } from '../Subscription';
import { OuterSubscriber } from '../OuterSubscriber';
export declare function subscribeToResult<T, R>(outerSubscriber: OuterSubscriber<T, R>, result: any, outerValue?: T, outerIndex?: number): Subscription;
