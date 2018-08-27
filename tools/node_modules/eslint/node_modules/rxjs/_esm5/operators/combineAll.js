/** PURE_IMPORTS_START .._operators_combineLatest PURE_IMPORTS_END */
import { CombineLatestOperator } from '../operators/combineLatest';
export function combineAll(project) {
    return function (source) { return source.lift(new CombineLatestOperator(project)); };
}
//# sourceMappingURL=combineAll.js.map
