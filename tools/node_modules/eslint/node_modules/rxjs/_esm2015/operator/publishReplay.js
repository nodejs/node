import { publishReplay as higherOrder } from '../operators/publishReplay';
/* tslint:enable:max-line-length */
/**
 * @param bufferSize
 * @param windowTime
 * @param selectorOrScheduler
 * @param scheduler
 * @return {Observable<T> | ConnectableObservable<T>}
 * @method publishReplay
 * @owner Observable
 */
export function publishReplay(bufferSize, windowTime, selectorOrScheduler, scheduler) {
    return higherOrder(bufferSize, windowTime, selectorOrScheduler, scheduler)(this);
}
//# sourceMappingURL=publishReplay.js.map