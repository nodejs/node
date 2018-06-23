/** PURE_IMPORTS_START ._zip PURE_IMPORTS_END */
import { ZipOperator } from './zip';
export function zipAll(project) {
    return function (source) { return source.lift(new ZipOperator(project)); };
}
//# sourceMappingURL=zipAll.js.map
