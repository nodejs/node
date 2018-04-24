'use strict';
const common = require('../../common');
const test_inheritance = require(`./build/${common.buildType}/test_inheritance`);

const classes = test_inheritance.createClasses();

const x = new classes.Subclass();

x.superclassMethod();
