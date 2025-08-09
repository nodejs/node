'use strict';
const { test } = require('node:test');

function coveredFunction() { 
    return 'This function is covered in coverage';
}

/* node:coverage ignore next 7 */
function ignoredFunctionDefinitionWithCount() {
    const ignoredVar = true;
    if (ignoredVar) {
        return 'This line is ignored';
    }
    return 'This should not be returned';
}

/* node:coverage disable */
function disabledFunction() {
    return 'This function is entirely disabled in coverage';
}
/* node:coverage enable */

test('coverage control comments fixture', () => {
    coveredFunction();
});
