"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
var jitter_factory_1 = require("../jitter/jitter.factory");
var Delay = /** @class */ (function () {
    function Delay(options) {
        this.options = options;
        this.attempt = 0;
    }
    Delay.prototype.apply = function () {
        var _this = this;
        return new Promise(function (resolve) { return setTimeout(resolve, _this.jitteredDelay); });
    };
    Delay.prototype.setAttemptNumber = function (attempt) {
        this.attempt = attempt;
    };
    Object.defineProperty(Delay.prototype, "jitteredDelay", {
        get: function () {
            var jitter = jitter_factory_1.JitterFactory(this.options);
            return jitter(this.delay);
        },
        enumerable: true,
        configurable: true
    });
    Object.defineProperty(Delay.prototype, "delay", {
        get: function () {
            var constant = this.options.startingDelay;
            var base = this.options.timeMultiple;
            var power = this.numOfDelayedAttempts;
            var delay = constant * Math.pow(base, power);
            return Math.min(delay, this.options.maxDelay);
        },
        enumerable: true,
        configurable: true
    });
    Object.defineProperty(Delay.prototype, "numOfDelayedAttempts", {
        get: function () {
            return this.attempt;
        },
        enumerable: true,
        configurable: true
    });
    return Delay;
}());
exports.Delay = Delay;
//# sourceMappingURL=delay.base.js.map