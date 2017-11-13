'use strict';
const common = require('../../common');
const assert = require('assert');

// Testing api calls for arrays
const test_dataview = require(`./build/${common.buildType}/test_dataview`);

//create dataview
const buffer = new ArrayBuffer(128);
const template = Reflect.construct(DataView, [buffer]);

const theDataview = test_dataview.CreateDataView(template);
assert.ok(theDataview instanceof DataView,
          `Expect ${theDataview} to be a DataView`);
