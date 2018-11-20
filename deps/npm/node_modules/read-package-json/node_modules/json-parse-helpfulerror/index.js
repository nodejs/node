'use strict';

var jju = require('jju');

function parse(text, reviver) {
    try {
        return JSON.parse(text, reviver);
    } catch (err) {
        // we expect this to throw with a more informative message
        jju.parse(text, {
            mode: 'json',
            reviver: reviver
        });

        // backup if jju is not as strict as JSON.parse; re-throw error
        // data-dependent code path, I do not know how to cover it
        throw err;
    }
}

exports.parse = parse;
