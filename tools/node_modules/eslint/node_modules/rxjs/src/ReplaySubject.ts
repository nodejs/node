import { Subject } from './Subject';
import { IScheduler } from './Scheduler';
import { queue } from './scheduler/queue';
import { Subscriber } from './Subscriber';
import { Subscription } from './Subscription';
import { ObserveOnSubscriber } from './operators/observeOn';
import { ObjectUnsubscribedError } from './util/ObjectUnsubscribedError';
import { SubjectSubscription } from './SubjectSubscription';
/**
 * @class ReplaySubject<T>
 */
export class ReplaySubject<T> extends Subject<T> {
  private _events: ReplayEvent<T>[] = [];
  private _bufferSize: number;
  private _windowTime: number;

  constructor(bufferSize: number = Number.POSITIVE_INFINITY,
              windowTime: number = Number.POSITIVE_INFINITY,
              private scheduler?: IScheduler) {
    super();
    this._bufferSize = bufferSize < 1 ? 1 : bufferSize;
    this._windowTime = windowTime < 1 ? 1 : windowTime;
  }

  next(value: T): void {
    const now = this._getNow();
    this._events.push(new ReplayEvent(now, value));
    this._trimBufferThenGetEvents();
    super.next(value);
  }

  /** @deprecated internal use only */ _subscribe(subscriber: Subscriber<T>): Subscription {
    const _events = this._trimBufferThenGetEvents();
    const scheduler = this.scheduler;
    let subscription: Subscription;

    if (this.closed) {
      throw new ObjectUnsubscribedError();
    } else if (this.hasError) {
      subscription = Subscription.EMPTY;
    } else if (this.isStopped) {
      subscription = Subscription.EMPTY;
    } else {
      this.observers.push(subscriber);
      subscription = new SubjectSubscription(this, subscriber);
    }

    if (scheduler) {
      subscriber.add(subscriber = new ObserveOnSubscriber<T>(subscriber, scheduler));
    }

    const len = _events.length;
    for (let i = 0; i < len && !subscriber.closed; i++) {
      subscriber.next(_events[i].value);
    }

    if (this.hasError) {
      subscriber.error(this.thrownError);
    } else if (this.isStopped) {
      subscriber.complete();
    }

    return subscription;
  }

  _getNow(): number {
    return (this.scheduler || queue).now();
  }

  private _trimBufferThenGetEvents(): ReplayEvent<T>[] {
    const now = this._getNow();
    const _bufferSize = this._bufferSize;
    const _windowTime = this._windowTime;
    const _events = this._events;

    let eventsCount = _events.length;
    let spliceCount = 0;

    // Trim events that fall out of the time window.
    // Start at the front of the list. Break early once
    // we encounter an event that falls within the window.
    while (spliceCount < eventsCount) {
      if ((now - _events[spliceCount].time) < _windowTime) {
        break;
      }
      spliceCount++;
    }

    if (eventsCount > _bufferSize) {
      spliceCount = Math.max(spliceCount, eventsCount - _bufferSize);
    }

    if (spliceCount > 0) {
      _events.splice(0, spliceCount);
    }

    return _events;
  }
}

class ReplayEvent<T> {
  constructor(public time: number, public value: T) {
  }
}
