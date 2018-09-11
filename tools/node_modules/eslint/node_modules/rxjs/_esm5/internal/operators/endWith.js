/** PURE_IMPORTS_START _observable_fromArray,_observable_scalar,_observable_empty,_observable_concat,_util_isScheduler PURE_IMPORTS_END */
import { fromArray } from '../observable/fromArray';
import { scalar } from '../observable/scalar';
import { empty } from '../observable/empty';
import { concat as concatStatic } from '../observable/concat';
import { isScheduler } from '../util/isScheduler';
export function endWith() {
    var array = [];
    for (var _i = 0; _i < arguments.length; _i++) {
        array[_i] = arguments[_i];
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
        if (len === 1 && !scheduler) {
            return concatStatic(source, scalar(array[0]));
        }
        else if (len > 0) {
            return concatStatic(source, fromArray(array, scheduler));
        }
        else {
            return concatStatic(source, empty(scheduler));
        }
    };
}
//# sourceMappingURL=endWith.js.map
