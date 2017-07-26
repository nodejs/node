/**
 * Created by robbin on 2017/7/25.
 */
'use strict';                                                          // 1
const common = require('../common');                                   // 2

// This test ensures that the http-parser can handle UTF-8 characters  // 4
// in the http header.                                                 // 5

const assert = require('assert');                                      // 7
const readline = require('../../lib/internal/readline');                                  // 8

assert.equal(readline.getStringWidth("hello"), 5);

assert.equal(readline.getStringWidth("hello world"), 11);


assert.equal(readline.getStringWidth("hello world!"), 12);