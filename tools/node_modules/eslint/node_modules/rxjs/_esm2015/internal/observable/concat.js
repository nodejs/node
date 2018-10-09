import { isScheduler } from '../util/isScheduler';
import { of } from './of';
import { from } from './from';
import { concatAll } from '../operators/concatAll';
export function concat(...observables) {
    if (observables.length === 1 || (observables.length === 2 && isScheduler(observables[1]))) {
        return from(observables[0]);
    }
    return concatAll()(of(...observables));
}
//# sourceMappingURL=concat.js.map