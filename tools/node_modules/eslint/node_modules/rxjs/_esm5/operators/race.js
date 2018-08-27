/** PURE_IMPORTS_START .._util_isArray,.._observable_race PURE_IMPORTS_END */
import { isArray } from '../util/isArray';
import { race as raceStatic } from '../observable/race';
/* tslint:enable:max-line-length */
/**
 * Returns an Observable that mirrors the first source Observable to emit an item
 * from the combination of this Observable and supplied Observables.
 * @param {...Observables} ...observables Sources used to race for which Observable emits first.
 * @return {Observable} An Observable that mirrors the output of the first Observable to emit an item.
 * @method race
 * @owner Observable
 */
export function race() {
    var observables = [];
    for (var _i = 0; _i < arguments.length; _i++) {
        observables[_i - 0] = arguments[_i];
    }
    return function raceOperatorFunction(source) {
        // if the only argument is an array, it was most likely called with
        // `pair([obs1, obs2, ...])`
        if (observables.length === 1 && isArray(observables[0])) {
            observables = observables[0];
        }
        return source.lift.call(raceStatic.apply(void 0, [source].concat(observables)));
    };
}
//# sourceMappingURL=race.js.map
