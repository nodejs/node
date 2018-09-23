"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
var Observable_1 = require("../Observable");
var Subscription_1 = require("../Subscription");
var observable_1 = require("../symbol/observable");
var subscribeToObservable_1 = require("../util/subscribeToObservable");
function fromObservable(input, scheduler) {
    if (!scheduler) {
        return new Observable_1.Observable(subscribeToObservable_1.subscribeToObservable(input));
    }
    else {
        return new Observable_1.Observable(function (subscriber) {
            var sub = new Subscription_1.Subscription();
            sub.add(scheduler.schedule(function () {
                var observable = input[observable_1.observable]();
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
exports.fromObservable = fromObservable;
//# sourceMappingURL=fromObservable.js.map