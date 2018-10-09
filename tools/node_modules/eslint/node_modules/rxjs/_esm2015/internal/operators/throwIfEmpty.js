import { tap } from './tap';
import { EmptyError } from '../util/EmptyError';
export const throwIfEmpty = (errorFactory = defaultErrorFactory) => tap({
    hasValue: false,
    next() { this.hasValue = true; },
    complete() {
        if (!this.hasValue) {
            throw errorFactory();
        }
    }
});
function defaultErrorFactory() {
    return new EmptyError();
}
//# sourceMappingURL=throwIfEmpty.js.map