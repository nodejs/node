import { isArray } from './util/isArray';
import { isObject } from './util/isObject';
import { isFunction } from './util/isFunction';
import { UnsubscriptionError } from './util/UnsubscriptionError';
import { SubscriptionLike, TeardownLogic } from './types';

/**
 * Represents a disposable resource, such as the execution of an Observable. A
 * Subscription has one important method, `unsubscribe`, that takes no argument
 * and just disposes the resource held by the subscription.
 *
 * Additionally, subscriptions may be grouped together through the `add()`
 * method, which will attach a child Subscription to the current Subscription.
 * When a Subscription is unsubscribed, all its children (and its grandchildren)
 * will be unsubscribed as well.
 *
 * @class Subscription
 */
export class Subscription implements SubscriptionLike {
  /** @nocollapse */
  public static EMPTY: Subscription = (function(empty: any) {
    empty.closed = true;
    return empty;
  }(new Subscription()));

  /**
   * A flag to indicate whether this Subscription has already been unsubscribed.
   * @type {boolean}
   */
  public closed: boolean = false;

  /** @internal */
  protected _parent: Subscription = null;
  /** @internal */
  protected _parents: Subscription[] = null;
  /** @internal */
  private _subscriptions: SubscriptionLike[] = null;

  /**
   * @param {function(): void} [unsubscribe] A function describing how to
   * perform the disposal of resources when the `unsubscribe` method is called.
   */
  constructor(unsubscribe?: () => void) {
    if (unsubscribe) {
      (<any> this)._unsubscribe = unsubscribe;
    }
  }

  /**
   * Disposes the resources held by the subscription. May, for instance, cancel
   * an ongoing Observable execution or cancel any other type of work that
   * started when the Subscription was created.
   * @return {void}
   */
  unsubscribe(): void {
    let hasErrors = false;
    let errors: any[];

    if (this.closed) {
      return;
    }

    let { _parent, _parents, _unsubscribe, _subscriptions } = (<any> this);

    this.closed = true;
    this._parent = null;
    this._parents = null;
    // null out _subscriptions first so any child subscriptions that attempt
    // to remove themselves from this subscription will noop
    this._subscriptions = null;

    let index = -1;
    let len = _parents ? _parents.length : 0;

    // if this._parent is null, then so is this._parents, and we
    // don't have to remove ourselves from any parent subscriptions.
    while (_parent) {
      _parent.remove(this);
      // if this._parents is null or index >= len,
      // then _parent is set to null, and the loop exits
      _parent = ++index < len && _parents[index] || null;
    }

    if (isFunction(_unsubscribe)) {
      try {
        _unsubscribe.call(this);
      } catch (e) {
        hasErrors = true;
        errors = e instanceof UnsubscriptionError ? flattenUnsubscriptionErrors(e.errors) : [e];
      }
    }

    if (isArray(_subscriptions)) {

      index = -1;
      len = _subscriptions.length;

      while (++index < len) {
        const sub = _subscriptions[index];
        if (isObject(sub)) {
          try {
            sub.unsubscribe();
          } catch (e) {
            hasErrors = true;
            errors = errors || [];
            if (e instanceof UnsubscriptionError) {
              errors = errors.concat(flattenUnsubscriptionErrors(e.errors));
            } else {
              errors.push(e);
            }
          }
        }
      }
    }

    if (hasErrors) {
      throw new UnsubscriptionError(errors);
    }
  }

  /**
   * Adds a tear down to be called during the unsubscribe() of this
   * Subscription. Can also be used to add a child subscription.
   *
   * If the tear down being added is a subscription that is already
   * unsubscribed, is the same reference `add` is being called on, or is
   * `Subscription.EMPTY`, it will not be added.
   *
   * If this subscription is already in an `closed` state, the passed
   * tear down logic will be executed immediately.
   *
   * When a parent subscription is unsubscribed, any child subscriptions that were added to it are also unsubscribed.
   *
   * @param {TeardownLogic} teardown The additional logic to execute on
   * teardown.
   * @return {Subscription} Returns the Subscription used or created to be
   * added to the inner subscriptions list. This Subscription can be used with
   * `remove()` to remove the passed teardown logic from the inner subscriptions
   * list.
   */
  add(teardown: TeardownLogic): Subscription {
    let subscription = (<Subscription>teardown);
    switch (typeof teardown) {
      case 'function':
        subscription = new Subscription(<(() => void)>teardown);
      case 'object':
        if (subscription === this || subscription.closed || typeof subscription.unsubscribe !== 'function') {
          // This also covers the case where `subscription` is `Subscription.EMPTY`, which is always in `closed` state.
          return subscription;
        } else if (this.closed) {
          subscription.unsubscribe();
          return subscription;
        } else if (!(subscription instanceof Subscription)) {
          const tmp = subscription;
          subscription = new Subscription();
          subscription._subscriptions = [tmp];
        }
        break;
      default: {
        if (!(<any>teardown)) {
          return Subscription.EMPTY;
        }
        throw new Error('unrecognized teardown ' + teardown + ' added to Subscription.');
      }
    }

    if (subscription._addParent(this)) {
      // Optimize for the common case when adding the first subscription.
      const subscriptions = this._subscriptions;
      if (subscriptions) {
        subscriptions.push(subscription);
      } else {
        this._subscriptions = [subscription];
      }
    }

    return subscription;
  }

  /**
   * Removes a Subscription from the internal list of subscriptions that will
   * unsubscribe during the unsubscribe process of this Subscription.
   * @param {Subscription} subscription The subscription to remove.
   * @return {void}
   */
  remove(subscription: Subscription): void {
    const subscriptions = this._subscriptions;
    if (subscriptions) {
      const subscriptionIndex = subscriptions.indexOf(subscription);
      if (subscriptionIndex !== -1) {
        subscriptions.splice(subscriptionIndex, 1);
      }
    }
  }

  /** @internal */
  private _addParent(parent: Subscription): boolean {
    let { _parent, _parents } = this;
    if (_parent === parent) {
      // If the new parent is the same as the current parent, then do nothing.
      return false;
    } else if (!_parent) {
      // If we don't have a parent, then set this._parent to the new parent.
      this._parent = parent;
      return true;
    } else if (!_parents) {
      // If there's already one parent, but not multiple, allocate an Array to
      // store the rest of the parent Subscriptions.
      this._parents = [parent];
      return true;
    } else if (_parents.indexOf(parent) === -1) {
      // Only add the new parent to the _parents list if it's not already there.
      _parents.push(parent);
      return true;
    }
    return false;
  }
}

function flattenUnsubscriptionErrors(errors: any[]) {
 return errors.reduce((errs, err) => errs.concat((err instanceof UnsubscriptionError) ? err.errors : err), []);
}
