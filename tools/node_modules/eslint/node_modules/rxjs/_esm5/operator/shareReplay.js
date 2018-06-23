/** PURE_IMPORTS_START .._operators_shareReplay PURE_IMPORTS_END */
import { shareReplay as higherOrder } from '../operators/shareReplay';
/**
 * @method shareReplay
 * @owner Observable
 */
export function shareReplay(bufferSize, windowTime, scheduler) {
    return higherOrder(bufferSize, windowTime, scheduler)(this);
}
;
//# sourceMappingURL=shareReplay.js.map
