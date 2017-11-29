'use strict';

function createError(msg, code, props) {
    var err = msg instanceof Error ? msg : new Error(msg);
    var key;

    if (typeof code === 'object') {
        props = code;
    } else if (code != null) {
        err.code = code;
    }

    if (props) {
        for (key in props) {
            err[key] = props[key];
        }
    }

    return err;
}

module.exports = createError;
