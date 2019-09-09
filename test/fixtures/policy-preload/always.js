'use strict';
console.log('from always.js');
const original = process.binding;
const allowed = {
    __proto__: null,
    'natives': true,
    'constants': true,
    'util': true,
    'tty_wrap': true
}
process.binding = function (name, ...args) {
    if (name in allowed !== true) {
        throw Error('No such module: ' + name);
    }
    return new.target ? new original(name, ...args) : original(name, ...args);
}
