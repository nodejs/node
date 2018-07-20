"use strict";
var BehaviorSubject_1 = require('../BehaviorSubject');
var multicast_1 = require('./multicast');
/**
 * @param value
 * @return {ConnectableObservable<T>}
 * @method publishBehavior
 * @owner Observable
 */
function publishBehavior(value) {
    return function (source) { return multicast_1.multicast(new BehaviorSubject_1.BehaviorSubject(value))(source); };
}
exports.publishBehavior = publishBehavior;
//# sourceMappingURL=publishBehavior.js.map