import { Observable } from '../Observable';
import { ScalarObservable } from './ScalarObservable';
import { EmptyObservable } from './EmptyObservable';
/**
 * We need this JSDoc comment for affecting ESDoc.
 * @extends {Ignored}
 * @hide true
 */
export class ArrayLikeObservable extends Observable {
    constructor(arrayLike, scheduler) {
        super();
        this.arrayLike = arrayLike;
        this.scheduler = scheduler;
        if (!scheduler && arrayLike.length === 1) {
            this._isScalar = true;
            this.value = arrayLike[0];
        }
    }
    static create(arrayLike, scheduler) {
        const length = arrayLike.length;
        if (length === 0) {
            return new EmptyObservable();
        }
        else if (length === 1) {
            return new ScalarObservable(arrayLike[0], scheduler);
        }
        else {
            return new ArrayLikeObservable(arrayLike, scheduler);
        }
    }
    static dispatch(state) {
        const { arrayLike, index, length, subscriber } = state;
        if (subscriber.closed) {
            return;
        }
        if (index >= length) {
            subscriber.complete();
            return;
        }
        subscriber.next(arrayLike[index]);
        state.index = index + 1;
        this.schedule(state);
    }
    /** @deprecated internal use only */ _subscribe(subscriber) {
        let index = 0;
        const { arrayLike, scheduler } = this;
        const length = arrayLike.length;
        if (scheduler) {
            return scheduler.schedule(ArrayLikeObservable.dispatch, 0, {
                arrayLike, index, length, subscriber
            });
        }
        else {
            for (let i = 0; i < length && !subscriber.closed; i++) {
                subscriber.next(arrayLike[i]);
            }
            subscriber.complete();
        }
    }
}
//# sourceMappingURL=ArrayLikeObservable.js.map