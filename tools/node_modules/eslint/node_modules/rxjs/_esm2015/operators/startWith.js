import { ArrayObservable } from '../observable/ArrayObservable';
import { ScalarObservable } from '../observable/ScalarObservable';
import { EmptyObservable } from '../observable/EmptyObservable';
import { concat as concatStatic } from '../observable/concat';
import { isScheduler } from '../util/isScheduler';
/* tslint:enable:max-line-length */
/**
 * Returns an Observable that emits the items you specify as arguments before it begins to emit
 * items emitted by the source Observable.
 *
 * <img src="./img/startWith.png" width="100%">
 *
 * @param {...T} values - Items you want the modified Observable to emit first.
 * @param {Scheduler} [scheduler] - A {@link IScheduler} to use for scheduling
 * the emissions of the `next` notifications.
 * @return {Observable} An Observable that emits the items in the specified Iterable and then emits the items
 * emitted by the source Observable.
 * @method startWith
 * @owner Observable
 */
export function startWith(...array) {
    return (source) => {
        let scheduler = array[array.length - 1];
        if (isScheduler(scheduler)) {
            array.pop();
        }
        else {
            scheduler = null;
        }
        const len = array.length;
        if (len === 1) {
            return concatStatic(new ScalarObservable(array[0], scheduler), source);
        }
        else if (len > 1) {
            return concatStatic(new ArrayObservable(array, scheduler), source);
        }
        else {
            return concatStatic(new EmptyObservable(scheduler), source);
        }
    };
}
//# sourceMappingURL=startWith.js.map