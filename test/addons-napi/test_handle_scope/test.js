'use strict';
const common = require('../../common');
const assert = require('assert');

// testing handle scope api calls
const test_handle_scope =
    require(`./build/${common.buildType}/test_handle_scope`);

test_handle_scope.NewScope();
assert.ok(test_handle_scope.NewScopeEscape() instanceof Object);
