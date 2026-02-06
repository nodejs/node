'use strict';
const common = require('../../common');
const assert = require('assert');

// Testing api calls for arrays
const test_dataview = require(`./build/${common.buildType}/test_dataview`);

// Test for creating dataview with ArrayBuffer
{
  const buffer = new ArrayBuffer(128);
  const template = Reflect.construct(DataView, [buffer]);

  const theDataview = test_dataview.CreateDataViewFromJSDataView(template);
  assert.ok(theDataview instanceof DataView,
            `Expect ${theDataview} to be a DataView`);
}

// Test for creating dataview with SharedArrayBuffer
{
  const buffer = new SharedArrayBuffer(128);
  const template = new DataView(buffer);

  const theDataview = test_dataview.CreateDataViewFromJSDataView(template);
  assert.ok(theDataview instanceof DataView,
            `Expect ${theDataview} to be a DataView`);

  assert.strictEqual(template.buffer, theDataview.buffer);
}

// Test for creating dataview with ArrayBuffer and invalid range
{
  const buffer = new ArrayBuffer(128);
  assert.throws(() => {
    test_dataview.CreateDataView(buffer, 10, 200);
  }, RangeError);
}

// Test for creating dataview with SharedArrayBuffer and invalid range
{
  const buffer = new SharedArrayBuffer(128);
  assert.throws(() => {
    test_dataview.CreateDataView(buffer, 10, 200);
  }, RangeError);
}
