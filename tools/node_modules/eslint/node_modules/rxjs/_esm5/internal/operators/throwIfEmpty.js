/** PURE_IMPORTS_START _tap,_util_EmptyError PURE_IMPORTS_END */
import { tap } from './tap';
import { EmptyError } from '../util/EmptyError';
export var throwIfEmpty = function (errorFactory) {
    if (errorFactory === void 0) {
        errorFactory = defaultErrorFactory;
    }
    return tap({
        hasValue: false,
        next: function () { this.hasValue = true; },
        complete: function () {
            if (!this.hasValue) {
                throw errorFactory();
            }
        }
    });
};
function defaultErrorFactory() {
    return new EmptyError();
}
//# sourceMappingURL=throwIfEmpty.js.map
