import { race as higherOrder } from '../operators/race';
// NOTE: to support backwards compatability with 5.4.* and lower
export { race as raceStatic } from '../observable/race';
/* tslint:enable:max-line-length */
/**
 * Returns an Observable that mirrors the first source Observable to emit an item
 * from the combination of this Observable and supplied Observables.
 * @param {...Observables} ...observables Sources used to race for which Observable emits first.
 * @return {Observable} An Observable that mirrors the output of the first Observable to emit an item.
 * @method race
 * @owner Observable
 */
export function race(...observables) {
    return higherOrder(...observables)(this);
}
//# sourceMappingURL=race.js.map