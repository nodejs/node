/** PURE_IMPORTS_START .._observable_ArrayObservable,.._observable_ScalarObservable,.._observable_EmptyObservable,.._observable_concat,.._util_isScheduler PURE_IMPORTS_END */
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
export function startWith() {
    var array = [];
    for (var _i = 0; _i < arguments.length; _i++) {
        array[_i - 0] = arguments[_i];
    }
    return function (source) {
        var scheduler = array[array.length - 1];
        if (isScheduler(scheduler)) {
            array.pop();
        }
        else {
            scheduler = null;
        }
        var len = array.length;
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
