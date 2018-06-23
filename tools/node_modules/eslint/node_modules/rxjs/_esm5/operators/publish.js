/** PURE_IMPORTS_START .._Subject,._multicast PURE_IMPORTS_END */
import { Subject } from '../Subject';
import { multicast } from './multicast';
/* tslint:enable:max-line-length */
/**
 * Returns a ConnectableObservable, which is a variety of Observable that waits until its connect method is called
 * before it begins emitting items to those Observers that have subscribed to it.
 *
 * <img src="./img/publish.png" width="100%">
 *
 * @param {Function} [selector] - Optional selector function which can use the multicasted source sequence as many times
 * as needed, without causing multiple subscriptions to the source sequence.
 * Subscribers to the given source will receive all notifications of the source from the time of the subscription on.
 * @return A ConnectableObservable that upon connection causes the source Observable to emit items to its Observers.
 * @method publish
 * @owner Observable
 */
export function publish(selector) {
    return selector ?
        multicast(function () { return new Subject(); }, selector) :
        multicast(new Subject());
}
//# sourceMappingURL=publish.js.map
