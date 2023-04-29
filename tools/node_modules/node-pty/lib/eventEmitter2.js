"use strict";
/**
 * Copyright (c) 2019, Microsoft Corporation (MIT License).
 */
Object.defineProperty(exports, "__esModule", { value: true });
var EventEmitter2 = /** @class */ (function () {
    function EventEmitter2() {
        this._listeners = [];
    }
    Object.defineProperty(EventEmitter2.prototype, "event", {
        get: function () {
            var _this = this;
            if (!this._event) {
                this._event = function (listener) {
                    _this._listeners.push(listener);
                    var disposable = {
                        dispose: function () {
                            for (var i = 0; i < _this._listeners.length; i++) {
                                if (_this._listeners[i] === listener) {
                                    _this._listeners.splice(i, 1);
                                    return;
                                }
                            }
                        }
                    };
                    return disposable;
                };
            }
            return this._event;
        },
        enumerable: true,
        configurable: true
    });
    EventEmitter2.prototype.fire = function (data) {
        var queue = [];
        for (var i = 0; i < this._listeners.length; i++) {
            queue.push(this._listeners[i]);
        }
        for (var i = 0; i < queue.length; i++) {
            queue[i].call(undefined, data);
        }
    };
    return EventEmitter2;
}());
exports.EventEmitter2 = EventEmitter2;
//# sourceMappingURL=eventEmitter2.js.map