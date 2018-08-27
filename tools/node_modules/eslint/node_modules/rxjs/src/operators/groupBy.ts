import { Subscriber } from '../Subscriber';
import { Subscription } from '../Subscription';
import { Observable } from '../Observable';
import { Operator } from '../Operator';
import { Subject } from '../Subject';
import { Map } from '../util/Map';
import { FastMap } from '../util/FastMap';
import { OperatorFunction } from '../interfaces';

/* tslint:disable:max-line-length */
export function groupBy<T, K>(keySelector: (value: T) => K): OperatorFunction<T, GroupedObservable<K, T>>;
export function groupBy<T, K>(keySelector: (value: T) => K, elementSelector: void, durationSelector: (grouped: GroupedObservable<K, T>) => Observable<any>): OperatorFunction<T, GroupedObservable<K, T>>;
export function groupBy<T, K, R>(keySelector: (value: T) => K, elementSelector?: (value: T) => R, durationSelector?: (grouped: GroupedObservable<K, R>) => Observable<any>): OperatorFunction<T, GroupedObservable<K, R>>;
export function groupBy<T, K, R>(keySelector: (value: T) => K, elementSelector?: (value: T) => R, durationSelector?: (grouped: GroupedObservable<K, R>) => Observable<any>, subjectSelector?: () => Subject<R>): OperatorFunction<T, GroupedObservable<K, R>>;
/* tslint:enable:max-line-length */

/**
 * Groups the items emitted by an Observable according to a specified criterion,
 * and emits these grouped items as `GroupedObservables`, one
 * {@link GroupedObservable} per group.
 *
 * <img src="./img/groupBy.png" width="100%">
 *
 * @example <caption>Group objects by id and return as array</caption>
 * Observable.of<Obj>({id: 1, name: 'aze1'},
 *                    {id: 2, name: 'sf2'},
 *                    {id: 2, name: 'dg2'},
 *                    {id: 1, name: 'erg1'},
 *                    {id: 1, name: 'df1'},
 *                    {id: 2, name: 'sfqfb2'},
 *                    {id: 3, name: 'qfs3'},
 *                    {id: 2, name: 'qsgqsfg2'}
 *     )
 *     .groupBy(p => p.id)
 *     .flatMap( (group$) => group$.reduce((acc, cur) => [...acc, cur], []))
 *     .subscribe(p => console.log(p));
 *
 * // displays:
 * // [ { id: 1, name: 'aze1' },
 * //   { id: 1, name: 'erg1' },
 * //   { id: 1, name: 'df1' } ]
 * //
 * // [ { id: 2, name: 'sf2' },
 * //   { id: 2, name: 'dg2' },
 * //   { id: 2, name: 'sfqfb2' },
 * //   { id: 2, name: 'qsgqsfg2' } ]
 * //
 * // [ { id: 3, name: 'qfs3' } ]
 *
 * @example <caption>Pivot data on the id field</caption>
 * Observable.of<Obj>({id: 1, name: 'aze1'},
 *                    {id: 2, name: 'sf2'},
 *                    {id: 2, name: 'dg2'},
 *                    {id: 1, name: 'erg1'},
 *                    {id: 1, name: 'df1'},
 *                    {id: 2, name: 'sfqfb2'},
 *                    {id: 3, name: 'qfs1'},
 *                    {id: 2, name: 'qsgqsfg2'}
 *                   )
 *     .groupBy(p => p.id, p => p.name)
 *     .flatMap( (group$) => group$.reduce((acc, cur) => [...acc, cur], ["" + group$.key]))
 *     .map(arr => ({'id': parseInt(arr[0]), 'values': arr.slice(1)}))
 *     .subscribe(p => console.log(p));
 *
 * // displays:
 * // { id: 1, values: [ 'aze1', 'erg1', 'df1' ] }
 * // { id: 2, values: [ 'sf2', 'dg2', 'sfqfb2', 'qsgqsfg2' ] }
 * // { id: 3, values: [ 'qfs1' ] }
 *
 * @param {function(value: T): K} keySelector A function that extracts the key
 * for each item.
 * @param {function(value: T): R} [elementSelector] A function that extracts the
 * return element for each item.
 * @param {function(grouped: GroupedObservable<K,R>): Observable<any>} [durationSelector]
 * A function that returns an Observable to determine how long each group should
 * exist.
 * @return {Observable<GroupedObservable<K,R>>} An Observable that emits
 * GroupedObservables, each of which corresponds to a unique key value and each
 * of which emits those items from the source Observable that share that key
 * value.
 * @method groupBy
 * @owner Observable
 */
export function groupBy<T, K, R>(keySelector: (value: T) => K,
                                 elementSelector?: ((value: T) => R) | void,
                                 durationSelector?: (grouped: GroupedObservable<K, R>) => Observable<any>,
                                 subjectSelector?: () => Subject<R>): OperatorFunction<T, GroupedObservable<K, R>> {
  return (source: Observable<T>) =>
    source.lift(new GroupByOperator(keySelector, elementSelector, durationSelector, subjectSelector));
}

export interface RefCountSubscription {
  count: number;
  unsubscribe: () => void;
  closed: boolean;
  attemptedToUnsubscribe: boolean;
}

class GroupByOperator<T, K, R> implements Operator<T, GroupedObservable<K, R>> {
  constructor(private keySelector: (value: T) => K,
              private elementSelector?: ((value: T) => R) | void,
              private durationSelector?: (grouped: GroupedObservable<K, R>) => Observable<any>,
              private subjectSelector?: () => Subject<R>) {
  }

  call(subscriber: Subscriber<GroupedObservable<K, R>>, source: any): any {
    return source.subscribe(new GroupBySubscriber(
      subscriber, this.keySelector, this.elementSelector, this.durationSelector, this.subjectSelector
    ));
  }
}

/**
 * We need this JSDoc comment for affecting ESDoc.
 * @ignore
 * @extends {Ignored}
 */
class GroupBySubscriber<T, K, R> extends Subscriber<T> implements RefCountSubscription {
  private groups: Map<K, Subject<T|R>> = null;
  public attemptedToUnsubscribe: boolean = false;
  public count: number = 0;

  constructor(destination: Subscriber<GroupedObservable<K, R>>,
              private keySelector: (value: T) => K,
              private elementSelector?: ((value: T) => R) | void,
              private durationSelector?: (grouped: GroupedObservable<K, R>) => Observable<any>,
              private subjectSelector?: () => Subject<R>) {
    super(destination);
  }

  protected _next(value: T): void {
    let key: K;
    try {
      key = this.keySelector(value);
    } catch (err) {
      this.error(err);
      return;
    }

    this._group(value, key);
  }

  private _group(value: T, key: K) {
    let groups = this.groups;

    if (!groups) {
      groups = this.groups = typeof key === 'string' ? new FastMap() : new Map();
    }

    let group = groups.get(key);

    let element: R;
    if (this.elementSelector) {
      try {
        element = this.elementSelector(value);
      } catch (err) {
        this.error(err);
      }
    } else {
      element = <any>value;
    }

    if (!group) {
      group = this.subjectSelector ? this.subjectSelector() : new Subject<R>();
      groups.set(key, group);
      const groupedObservable = new GroupedObservable(key, group, this);
      this.destination.next(groupedObservable);
      if (this.durationSelector) {
        let duration: any;
        try {
          duration = this.durationSelector(new GroupedObservable<K, R>(key, <Subject<R>>group));
        } catch (err) {
          this.error(err);
          return;
        }
        this.add(duration.subscribe(new GroupDurationSubscriber(key, group, this)));
      }
    }

    if (!group.closed) {
      group.next(element);
    }
  }

  protected _error(err: any): void {
    const groups = this.groups;
    if (groups) {
      groups.forEach((group, key) => {
        group.error(err);
      });

      groups.clear();
    }
    this.destination.error(err);
  }

  protected _complete(): void {
    const groups = this.groups;
    if (groups) {
      groups.forEach((group, key) => {
        group.complete();
      });

      groups.clear();
    }
    this.destination.complete();
  }

  removeGroup(key: K): void {
    this.groups.delete(key);
  }

  unsubscribe() {
    if (!this.closed) {
      this.attemptedToUnsubscribe = true;
      if (this.count === 0) {
        super.unsubscribe();
      }
    }
  }
}

/**
 * We need this JSDoc comment for affecting ESDoc.
 * @ignore
 * @extends {Ignored}
 */
class GroupDurationSubscriber<K, T> extends Subscriber<T> {
  constructor(private key: K,
              private group: Subject<T>,
              private parent: GroupBySubscriber<any, K, T>) {
    super(group);
  }

  protected _next(value: T): void {
    this.complete();
  }

  /** @deprecated internal use only */ _unsubscribe() {
    const { parent, key } = this;
    this.key = this.parent = null;
    if (parent) {
      parent.removeGroup(key);
    }
  }
}

/**
 * An Observable representing values belonging to the same group represented by
 * a common key. The values emitted by a GroupedObservable come from the source
 * Observable. The common key is available as the field `key` on a
 * GroupedObservable instance.
 *
 * @class GroupedObservable<K, T>
 */
export class GroupedObservable<K, T> extends Observable<T> {
  constructor(public key: K,
              private groupSubject: Subject<T>,
              private refCountSubscription?: RefCountSubscription) {
    super();
  }

  /** @deprecated internal use only */ _subscribe(subscriber: Subscriber<T>) {
    const subscription = new Subscription();
    const {refCountSubscription, groupSubject} = this;
    if (refCountSubscription && !refCountSubscription.closed) {
      subscription.add(new InnerRefCountSubscription(refCountSubscription));
    }
    subscription.add(groupSubject.subscribe(subscriber));
    return subscription;
  }
}

/**
 * We need this JSDoc comment for affecting ESDoc.
 * @ignore
 * @extends {Ignored}
 */
class InnerRefCountSubscription extends Subscription {
  constructor(private parent: RefCountSubscription) {
    super();
    parent.count++;
  }

  unsubscribe() {
    const parent = this.parent;
    if (!parent.closed && !this.closed) {
      super.unsubscribe();
      parent.count -= 1;
      if (parent.count === 0 && parent.attemptedToUnsubscribe) {
        parent.unsubscribe();
      }
    }
  }
}
