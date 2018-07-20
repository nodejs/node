import { OuterSubscriber } from '../OuterSubscriber';
import { subscribeToResult } from '../util/subscribeToResult';
/**
 * Returns an Observable that skips items emitted by the source Observable until a second Observable emits an item.
 *
 * <img src="./img/skipUntil.png" width="100%">
 *
 * @param {Observable} notifier - The second Observable that has to emit an item before the source Observable's elements begin to
 * be mirrored by the resulting Observable.
 * @return {Observable<T>} An Observable that skips items from the source Observable until the second Observable emits
 * an item, then emits the remaining items.
 * @method skipUntil
 * @owner Observable
 */
export function skipUntil(notifier) {
    return (source) => source.lift(new SkipUntilOperator(notifier));
}
class SkipUntilOperator {
    constructor(notifier) {
        this.notifier = notifier;
    }
    call(subscriber, source) {
        return source.subscribe(new SkipUntilSubscriber(subscriber, this.notifier));
    }
}
/**
 * We need this JSDoc comment for affecting ESDoc.
 * @ignore
 * @extends {Ignored}
 */
class SkipUntilSubscriber extends OuterSubscriber {
    constructor(destination, notifier) {
        super(destination);
        this.hasValue = false;
        this.isInnerStopped = false;
        this.add(subscribeToResult(this, notifier));
    }
    _next(value) {
        if (this.hasValue) {
            super._next(value);
        }
    }
    _complete() {
        if (this.isInnerStopped) {
            super._complete();
        }
        else {
            this.unsubscribe();
        }
    }
    notifyNext(outerValue, innerValue, outerIndex, innerIndex, innerSub) {
        this.hasValue = true;
    }
    notifyComplete() {
        this.isInnerStopped = true;
        if (this.isStopped) {
            super._complete();
        }
    }
}
//# sourceMappingURL=skipUntil.js.map