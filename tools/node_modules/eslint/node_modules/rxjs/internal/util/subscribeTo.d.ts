import { ObservableInput } from '../types';
import { Subscriber } from '../Subscriber';
export declare const subscribeTo: <T>(result: ObservableInput<T>) => (subscriber: Subscriber<{}>) => any;
