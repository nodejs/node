// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Checks Runtime.getProperties method');

InspectorTest.runAsyncTestSuite([
  async function testObject5() {
    let objectId = (await Protocol.Runtime.evaluate({
      expression: '(function(){var r = Object(5); r.foo = \'cat\';return r;})()'
    })).result.result.objectId;
    let props = await Protocol.Runtime.getProperties({ objectId, ownProperties: true });
    logGetPropertiesResult(props.result);
  },

  async function testNotOwn() {
    let objectId = (await Protocol.Runtime.evaluate({
      expression: '({ a: 2, set b(_) {}, get b() {return 5;}, __proto__: { a: 3, c: 4, get d() {return 6;} }})'
    })).result.result.objectId;
    let props = await Protocol.Runtime.getProperties({ objectId, ownProperties: false });
    logGetPropertiesResult(props.result);
  },

  async function testAccessorsOnly() {
    let objectId = (await Protocol.Runtime.evaluate({
      expression: '({ a: 2, set b(_) {}, get b() {return 5;}, c: \'c\', set d(_){} })'
    })).result.result.objectId;
    let props = await Protocol.Runtime.getProperties({ objectId, ownProperties: true, accessorPropertiesOnly: true });
    logGetPropertiesResult(props.result);
  },

  async function testArray() {
    let objectId = (await Protocol.Runtime.evaluate({
      expression: '[\'red\', \'green\', \'blue\']'
    })).result.result.objectId;
    let props = await Protocol.Runtime.getProperties({ objectId, ownProperties: true });
    logGetPropertiesResult(props.result);
  },

  async function testBound() {
    let objectId = (await Protocol.Runtime.evaluate({
      expression: 'Number.bind({}, 5)'
    })).result.result.objectId;
    let props = await Protocol.Runtime.getProperties({ objectId, ownProperties: true });
    logGetPropertiesResult(props.result);
  },

  async function testObjectThrowsLength() {
    let objectId = (await Protocol.Runtime.evaluate({
      expression: '({get length() { throw \'Length called\'; }})'
    })).result.result.objectId;
    let props = await Protocol.Runtime.getProperties({ objectId, ownProperties: true });
    logGetPropertiesResult(props.result);
  },

  async function testTypedArrayWithoutLength() {
    let objectId = (await Protocol.Runtime.evaluate({
      expression: '({__proto__: Uint8Array.prototype})'
    })).result.result.objectId;
    let props = await Protocol.Runtime.getProperties({ objectId, ownProperties: true });
    logGetPropertiesResult(props.result);
  },
]);

function logGetPropertiesResult(protocolResult) {
  function hasGetterSetter(property, fieldName) {
    var v = property[fieldName];
    if (!v) return false;
    return v.type !== "undefined"
  }

  var propertyArray = protocolResult.result;
  propertyArray.sort(NamedThingComparator);
  for (var i = 0; i < propertyArray.length; i++) {
    var p = propertyArray[i];
    var v = p.value;
    var own = p.isOwn ? "own" : "inherited";
    if (v)
      InspectorTest.log("  " + p.name + " " + own + " " + v.type + " " + v.value);
    else
      InspectorTest.log("  " + p.name + " " + own + " no value" +
        (hasGetterSetter(p, "get") ? ", getter" : "") + (hasGetterSetter(p, "set") ? ", setter" : ""));
  }
  var internalPropertyArray = protocolResult.internalProperties;
  if (internalPropertyArray) {
    InspectorTest.log("Internal properties");
    internalPropertyArray.sort(NamedThingComparator);
    for (var i = 0; i < internalPropertyArray.length; i++) {
      var p = internalPropertyArray[i];
      var v = p.value;
      InspectorTest.log("  " + p.name + " " + v.type + " " + v.value);
    }
  }

  function NamedThingComparator(o1, o2) {
    return o1.name === o2.name ? 0 : (o1.name < o2.name ? -1 : 1);
  }
}
