// This file intentionally mixes CommonJS and ES Module syntax
// to trigger ERR_AMBIGUOUS_MODULE_SYNTAX.

// CommonJS
const fs = require('fs');

// ES Modules
export const value = 123;
