"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.callback = exports.promise = void 0;
const walker_1 = require("./walker");
function promise(root, options) {
    return new Promise((resolve, reject) => {
        callback(root, options, (err, output) => {
            if (err)
                return reject(err);
            resolve(output);
        });
    });
}
exports.promise = promise;
function callback(root, options, callback) {
    let walker = new walker_1.Walker(root, options, callback);
    walker.start();
}
exports.callback = callback;
