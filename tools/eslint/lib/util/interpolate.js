/**
 * @fileoverview Interpolate keys from an object into a string with {{ }} markers.
 * @author Jed Fox
 */

"use strict";

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

module.exports = (text, data) => {
    if (!data) {
        return text;
    }
    return text.replace(/\{\{\s*([^{}]+?)\s*\}\}/g, (fullMatch, term) => {
        if (term in data) {
            return data[term];
        }

        // Preserve old behavior: If parameter name not provided, don't replace it.
        return fullMatch;
    });
};
