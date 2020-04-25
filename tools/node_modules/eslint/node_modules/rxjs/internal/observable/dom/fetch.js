"use strict";
var __assign = (this && this.__assign) || function () {
    __assign = Object.assign || function(t) {
        for (var s, i = 1, n = arguments.length; i < n; i++) {
            s = arguments[i];
            for (var p in s) if (Object.prototype.hasOwnProperty.call(s, p))
                t[p] = s[p];
        }
        return t;
    };
    return __assign.apply(this, arguments);
};
Object.defineProperty(exports, "__esModule", { value: true });
var Observable_1 = require("../../Observable");
var Subscription_1 = require("../../Subscription");
function fromFetch(input, init) {
    return new Observable_1.Observable(function (subscriber) {
        var controller = new AbortController();
        var signal = controller.signal;
        var abortable = true;
        var unsubscribed = false;
        var subscription = new Subscription_1.Subscription();
        subscription.add(function () {
            unsubscribed = true;
            if (abortable) {
                controller.abort();
            }
        });
        var perSubscriberInit;
        if (init) {
            if (init.signal) {
                if (init.signal.aborted) {
                    controller.abort();
                }
                else {
                    var outerSignal_1 = init.signal;
                    var outerSignalHandler_1 = function () {
                        if (!signal.aborted) {
                            controller.abort();
                        }
                    };
                    outerSignal_1.addEventListener('abort', outerSignalHandler_1);
                    subscription.add(function () { return outerSignal_1.removeEventListener('abort', outerSignalHandler_1); });
                }
            }
            perSubscriberInit = __assign({}, init, { signal: signal });
        }
        else {
            perSubscriberInit = { signal: signal };
        }
        fetch(input, perSubscriberInit).then(function (response) {
            abortable = false;
            subscriber.next(response);
            subscriber.complete();
        }).catch(function (err) {
            abortable = false;
            if (!unsubscribed) {
                subscriber.error(err);
            }
        });
        return subscription;
    });
}
exports.fromFetch = fromFetch;
//# sourceMappingURL=fetch.js.map