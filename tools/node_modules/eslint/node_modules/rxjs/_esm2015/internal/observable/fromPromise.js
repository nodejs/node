import { Observable } from '../Observable';
import { Subscription } from '../Subscription';
import { subscribeToPromise } from '../util/subscribeToPromise';
export function fromPromise(input, scheduler) {
    if (!scheduler) {
        return new Observable(subscribeToPromise(input));
    }
    else {
        return new Observable(subscriber => {
            const sub = new Subscription();
            sub.add(scheduler.schedule(() => input.then(value => {
                sub.add(scheduler.schedule(() => {
                    subscriber.next(value);
                    sub.add(scheduler.schedule(() => subscriber.complete()));
                }));
            }, err => {
                sub.add(scheduler.schedule(() => subscriber.error(err)));
            })));
            return sub;
        });
    }
}
//# sourceMappingURL=fromPromise.js.map