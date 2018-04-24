'use strict';
const common = require('../../common');
const test_inheritance = require(`./build/${common.buildType}/test_inheritance`);
const x = new test_inheritance.Subclass();
x.superclassMethod();
