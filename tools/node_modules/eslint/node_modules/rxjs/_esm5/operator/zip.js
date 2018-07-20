/** PURE_IMPORTS_START .._operators_zip PURE_IMPORTS_END */
import { zip as higherOrder } from '../operators/zip';
/* tslint:enable:max-line-length */
/**
 * @param observables
 * @return {Observable<R>}
 * @method zip
 * @owner Observable
 */
export function zipProto() {
    var observables = [];
    for (var _i = 0; _i < arguments.length; _i++) {
        observables[_i - 0] = arguments[_i];
    }
    return higherOrder.apply(void 0, observables)(this);
}
//# sourceMappingURL=zip.js.map
