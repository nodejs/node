import { fromArray } from '../observable/fromArray';
import { scalar } from '../observable/scalar';
import { empty } from '../observable/empty';
import { concat as concatStatic } from '../observable/concat';
import { isScheduler } from '../util/isScheduler';
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
        if (len === 1 && !scheduler) {
            return concatStatic(scalar(array[0]), source);
        }
        else if (len > 0) {
            return concatStatic(fromArray(array, scheduler), source);
        }
        else {
            return concatStatic(empty(scheduler), source);
        }
    };
}
//# sourceMappingURL=startWith.js.map