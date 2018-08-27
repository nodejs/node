/** PURE_IMPORTS_START .._operators_zipAll PURE_IMPORTS_END */
import { zipAll as higherOrder } from '../operators/zipAll';
/**
 * @param project
 * @return {Observable<R>|WebSocketSubject<T>|Observable<T>}
 * @method zipAll
 * @owner Observable
 */
export function zipAll(project) {
    return higherOrder(project)(this);
}
//# sourceMappingURL=zipAll.js.map
