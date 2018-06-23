import { Observable, SubscribableOrPromise } from '../Observable';
import { Subscriber } from '../Subscriber';
import { AnonymousSubscription, TeardownLogic } from '../Subscription';

import { subscribeToResult } from '../util/subscribeToResult';
import { OuterSubscriber } from '../OuterSubscriber';
/**
 * We need this JSDoc comment for affecting ESDoc.
 * @extends {Ignored}
 * @hide true
 */
export class UsingObservable<T> extends Observable<T> {

  static create<T>(resourceFactory: () => AnonymousSubscription | void,
                   observableFactory: (resource: AnonymousSubscription) => SubscribableOrPromise<T> | void): Observable<T> {
    return new UsingObservable<T>(resourceFactory, observableFactory);
  }

  constructor(private resourceFactory: () => AnonymousSubscription | void,
              private observableFactory: (resource: AnonymousSubscription) => SubscribableOrPromise<T> | void) {
    super();
  }

  /** @deprecated internal use only */ _subscribe(subscriber: Subscriber<T>): TeardownLogic {
    const { resourceFactory, observableFactory } = this;

    let resource: AnonymousSubscription;

    try {
      resource = <AnonymousSubscription>resourceFactory();
      return new UsingSubscriber(subscriber, resource, observableFactory);
    } catch (err) {
      subscriber.error(err);
    }
  }
}

class UsingSubscriber<T> extends OuterSubscriber<T, T> {
  constructor(destination: Subscriber<T>,
              private resource: AnonymousSubscription,
              private observableFactory: (resource: AnonymousSubscription) => SubscribableOrPromise<T> | void) {
    super(destination);
    destination.add(resource);
    this.tryUse();
  }

  private tryUse(): void {
    try {
      const source = this.observableFactory.call(this, this.resource);
      if (source) {
        this.add(subscribeToResult(this, source));
      }
    } catch (err) {
      this._error(err);
    }
  }
}
