import { isEmpty as higherOrder } from '../operators/isEmpty';
/**
 * If the source Observable is empty it returns an Observable that emits true, otherwise it emits false.
 *
 * <img src="./img/isEmpty.png" width="100%">
 *
 * @return {Observable} An Observable that emits a Boolean.
 * @method isEmpty
 * @owner Observable
 */
export function isEmpty() {
    return higherOrder()(this);
}
//# sourceMappingURL=isEmpty.js.map