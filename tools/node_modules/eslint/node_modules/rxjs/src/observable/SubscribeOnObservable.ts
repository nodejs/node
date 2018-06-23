import { Action } from '../scheduler/Action';
import { IScheduler } from '../Scheduler';
import { Subscriber } from '../Subscriber';
import { Subscription } from '../Subscription';
import { Observable } from '../Observable';
import { asap } from '../scheduler/asap';
import { isNumeric } from '../util/isNumeric';

export interface DispatchArg<T> {
  source: Observable<T>;
  subscriber: Subscriber<T>;
}

/**
 * We need this JSDoc comment for affecting ESDoc.
 * @extends {Ignored}
 * @hide true
 */
export class SubscribeOnObservable<T> extends Observable<T> {
  static create<T>(source: Observable<T>, delay: number = 0, scheduler: IScheduler = asap): Observable<T> {
    return new SubscribeOnObservable(source, delay, scheduler);
  }

  static dispatch<T>(this: Action<T>, arg: DispatchArg<T>): Subscription {
    const { source, subscriber } = arg;
    return this.add(source.subscribe(subscriber));
  }

  constructor(public source: Observable<T>,
              private delayTime: number = 0,
              private scheduler: IScheduler = asap) {
    super();
    if (!isNumeric(delayTime) || delayTime < 0) {
      this.delayTime = 0;
    }
    if (!scheduler || typeof scheduler.schedule !== 'function') {
      this.scheduler = asap;
    }
  }

  /** @deprecated internal use only */ _subscribe(subscriber: Subscriber<T>) {
    const delay = this.delayTime;
    const source = this.source;
    const scheduler = this.scheduler;

    return scheduler.schedule(SubscribeOnObservable.dispatch, delay, {
      source, subscriber
    });
  }
}
