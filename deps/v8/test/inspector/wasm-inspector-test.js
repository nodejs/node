// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/mjsunit/wasm/wasm-module-builder.js');

WasmInspectorTest = {}
InspectorTest.getWasmOpcodeName = getOpcodeName;

WasmInspectorTest.evalWithUrl = (code, url) =>
    Protocol.Runtime
        .evaluate({'expression': code + '\n//# sourceURL=v8://test/' + url})
        .then(printIfFailure);

WasmInspectorTest.instantiateFromBuffer = function(bytes, imports) {
  var buffer = new ArrayBuffer(bytes.length);
  var view = new Uint8Array(buffer);
  for (var i = 0; i < bytes.length; ++i) {
    view[i] = bytes[i] | 0;
  }
  const module = new WebAssembly.Module(buffer);
  return new WebAssembly.Instance(module, imports);
}

WasmInspectorTest.instantiate = async function(bytes, instance_name = 'instance') {
  const instantiate_code = `var ${instance_name} = (${WasmInspectorTest.instantiateFromBuffer})(${JSON.stringify(bytes)});`;
  await WasmInspectorTest.evalWithUrl(instantiate_code, 'instantiate');
}

WasmInspectorTest.dumpScopeProperties = async function(message) {
  printIfFailure(message);
  for (var value of message.result.result) {
    var value_str = await getScopeValues(value.name, value.value);
    InspectorTest.log('   ' + value.name + ': ' + value_str);
  }
}

WasmInspectorTest.getWasmValue = value => {
  return value.unserializableValue ?? value.value;
}

function printIfFailure(message) {
  if (!message.result) {
    InspectorTest.logMessage(message);
  }
  return message;
}

async function getScopeValues(name, value) {
  if (value.type == 'object') {
    if (value.subtype === 'typedarray' || value.subtype == 'webassemblymemory') return value.description;
    if (name === 'instance') return dumpInstanceProperties(value);
    if (name === 'module') return value.description;

    let msg = await Protocol.Runtime.getProperties({objectId: value.objectId});
    printIfFailure(msg);
    const printProperty = function({name, value}) {
      return `"${name}": ${WasmInspectorTest.getWasmValue(value)} (${value.subtype ?? value.type})`;
    }
    return msg.result.result.map(printProperty).join(', ');
  }
  return `${WasmInspectorTest.getWasmValue(value)} (${value.subtype ?? value.type})`;
}

function recursiveGetPropertiesWrapper(value, depth) {
  return recursiveGetProperties({result: {result: [value]}}, depth);
}

async function recursiveGetProperties(value, depth) {
  if (depth > 0) {
    const properties = await Promise.all(value.result.result.map(
        x => {return Protocol.Runtime.getProperties({objectId: x.value.objectId});}));
    const recursiveProperties = await Promise.all(properties.map(
        x => {return recursiveGetProperties(x, depth - 1);}));
    return recursiveProperties.flat();
  }
  return value;
}

async function dumpInstanceProperties(instanceObj) {
  function invokeGetter(property) {
    return this[JSON.parse(property)];
  }

  const exportsName = 'exports';
  let exportsObj = await Protocol.Runtime.callFunctionOn(
      {objectId: instanceObj.objectId,
      functionDeclaration: invokeGetter.toString(),
      arguments: [{value: JSON.stringify(exportsName)}]
    });
  printIfFailure(exportsObj);
  let exports = await Protocol.Runtime.getProperties(
      {objectId: exportsObj.result.result.objectId});
  printIfFailure(exports);

  const printExports = function(value) {
    return `"${value.name}" (${value.value.className})`;
  }
  const formattedExports = exports.result.result.map(printExports).join(', ');
  return `${exportsName}: ${formattedExports}`
}
