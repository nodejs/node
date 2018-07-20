"use strict";
var async_1 = require('../scheduler/async');
var isNumeric_1 = require('../util/isNumeric');
var isScheduler_1 = require('../util/isScheduler');
var windowTime_1 = require('../operators/windowTime');
function windowTime(windowTimeSpan) {
    var scheduler = async_1.async;
    var windowCreationInterval = null;
    var maxWindowSize = Number.POSITIVE_INFINITY;
    if (isScheduler_1.isScheduler(arguments[3])) {
        scheduler = arguments[3];
    }
    if (isScheduler_1.isScheduler(arguments[2])) {
        scheduler = arguments[2];
    }
    else if (isNumeric_1.isNumeric(arguments[2])) {
        maxWindowSize = arguments[2];
    }
    if (isScheduler_1.isScheduler(arguments[1])) {
        scheduler = arguments[1];
    }
    else if (isNumeric_1.isNumeric(arguments[1])) {
        windowCreationInterval = arguments[1];
    }
    return windowTime_1.windowTime(windowTimeSpan, windowCreationInterval, maxWindowSize, scheduler)(this);
}
exports.windowTime = windowTime;
//# sourceMappingURL=windowTime.js.map