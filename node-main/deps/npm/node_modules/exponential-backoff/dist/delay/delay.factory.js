"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
var skip_first_delay_1 = require("./skip-first/skip-first.delay");
var always_delay_1 = require("./always/always.delay");
function DelayFactory(options, attempt) {
    var delay = initDelayClass(options);
    delay.setAttemptNumber(attempt);
    return delay;
}
exports.DelayFactory = DelayFactory;
function initDelayClass(options) {
    if (!options.delayFirstAttempt) {
        return new skip_first_delay_1.SkipFirstDelay(options);
    }
    return new always_delay_1.AlwaysDelay(options);
}
//# sourceMappingURL=delay.factory.js.map