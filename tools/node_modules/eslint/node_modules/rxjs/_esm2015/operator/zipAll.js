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