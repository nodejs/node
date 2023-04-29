"use strict";
/**
 * Copyright (c) 2019, Microsoft Corporation (MIT License).
 */
Object.defineProperty(exports, "__esModule", { value: true });
function pollUntil(cb, timeout, interval) {
    return new Promise(function (resolve, reject) {
        var intervalId = setInterval(function () {
            if (cb()) {
                clearInterval(intervalId);
                clearTimeout(timeoutId);
                resolve();
            }
        }, interval);
        var timeoutId = setTimeout(function () {
            clearInterval(intervalId);
            if (cb()) {
                resolve();
            }
            else {
                reject();
            }
        }, timeout);
    });
}
exports.pollUntil = pollUntil;
//# sourceMappingURL=testUtils.test.js.map