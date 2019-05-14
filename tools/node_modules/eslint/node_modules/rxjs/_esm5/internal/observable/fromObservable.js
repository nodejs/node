/** PURE_IMPORTS_START _Observable,_Subscription,_symbol_observable,_util_subscribeToObservable PURE_IMPORTS_END */
import { Observable } from '../Observable';
import { Subscription } from '../Subscription';
import { observable as Symbol_observable } from '../symbol/observable';
import { subscribeToObservable } from '../util/subscribeToObservable';
export function fromObservable(input, scheduler) {
    if (!scheduler) {
        return new Observable(subscribeToObservable(input));
    }
    else {
        return new Observable(function (subscriber) {
            var sub = new Subscription();
            sub.add(scheduler.schedule(function () {
                var observable = input[Symbol_observable]();
                sub.add(observable.subscribe({
                    next: function (value) { sub.add(scheduler.schedule(function () { return subscriber.next(value); })); },
                    error: function (err) { sub.add(scheduler.schedule(function () { return subscriber.error(err); })); },
                    complete: function () { sub.add(scheduler.schedule(function () { return subscriber.complete(); })); },
                }));
            }));
            return sub;
        });
    }
}
//# sourceMappingURL=fromObservable.js.map
