/**
 * @fileoverview Enables the processor for Markdown file extensions.
 * @author Brandon Mills
 */

"use strict";

var processor = require("./processor");

module.exports = {
    "processors": {
        ".markdown": processor,
        ".mdown": processor,
        ".mkdn": processor,
        ".md": processor
    }
};
