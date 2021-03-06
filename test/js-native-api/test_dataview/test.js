'use strict';
const common = require('../../common');
const assert = require('assert');

// Testing api calls for arrays
const test_dataview = require(`./build/${common.buildType}/test_dataview`);

// Test for creating dataview
{
  const buffer = new ArrayBuffer(128);
  const template = Reflect.construct(DataView, [buffer]);

  const theDataview = test_dataview.CreateDataViewFromJSDataView(template);
  assert.ok(theDataview instanceof DataView,
            `Expect ${theDataview} to be a DataView`);
}

// Test for creating dataview with invalid range
{
  const buffer = new ArrayBuffer(128);
  assert.throws(() => {
    test_dataview.CreateDataView(buffer, 10, 200);
  }, RangeError);
}
