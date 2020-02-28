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
function fromFetch(input, init) {
    return new Observable_1.Observable(function (subscriber) {
        var controller = new AbortController();
        var signal = controller.signal;
        var outerSignalHandler;
        var abortable = true;
        var unsubscribed = false;
        if (init) {
            if (init.signal) {
                if (init.signal.aborted) {
                    controller.abort();
                }
                else {
                    outerSignalHandler = function () {
                        if (!signal.aborted) {
                            controller.abort();
                        }
                    };
                    init.signal.addEventListener('abort', outerSignalHandler);
                }
            }
            init = __assign({}, init, { signal: signal });
        }
        else {
            init = { signal: signal };
        }
        fetch(input, init).then(function (response) {
            abortable = false;
            subscriber.next(response);
            subscriber.complete();
        }).catch(function (err) {
            abortable = false;
            if (!unsubscribed) {
                subscriber.error(err);
            }
        });
        return function () {
            unsubscribed = true;
            if (abortable) {
                controller.abort();
            }
        };
    });
}
exports.fromFetch = fromFetch;
//# sourceMappingURL=fetch.js.map