import { fromArray } from '../observable/fromArray';
import { scalar } from '../observable/scalar';
import { empty } from '../observable/empty';
import { concat as concatStatic } from '../observable/concat';
import { isScheduler } from '../util/isScheduler';
export function endWith(...array) {
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