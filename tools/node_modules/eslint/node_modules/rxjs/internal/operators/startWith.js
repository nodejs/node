"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
var fromArray_1 = require("../observable/fromArray");
var scalar_1 = require("../observable/scalar");
var empty_1 = require("../observable/empty");
var concat_1 = require("../observable/concat");
var isScheduler_1 = require("../util/isScheduler");
function startWith() {
    var array = [];
    for (var _i = 0; _i < arguments.length; _i++) {
        array[_i] = arguments[_i];
    }
    return function (source) {
        var scheduler = array[array.length - 1];
        if (isScheduler_1.isScheduler(scheduler)) {
            array.pop();
        }
        else {
            scheduler = null;
        }
        var len = array.length;
        if (len === 1 && !scheduler) {
            return concat_1.concat(scalar_1.scalar(array[0]), source);
        }
        else if (len > 0) {
            return concat_1.concat(fromArray_1.fromArray(array, scheduler), source);
        }
        else {
            return concat_1.concat(empty_1.empty(scheduler), source);
        }
    };
}
exports.startWith = startWith;
//# sourceMappingURL=startWith.js.map