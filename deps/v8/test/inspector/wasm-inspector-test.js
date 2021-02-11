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

WasmInspectorTest.instantiateFromBuffer = function(bytes) {
  var buffer = new ArrayBuffer(bytes.length);
  var view = new Uint8Array(buffer);
  for (var i = 0; i < bytes.length; ++i) {
    view[i] = bytes[i] | 0;
  }
  const module = new WebAssembly.Module(buffer);
  return new WebAssembly.Instance(module);
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

WasmInspectorTest.getWasmValue = function(wasmValue) {
  return typeof (wasmValue.value) === 'undefined' ?
      wasmValue.unserializableValue :
      wasmValue.value;
}

function printIfFailure(message) {
  if (!message.result) {
    InspectorTest.logMessage(message);
  }
  return message;
}

async function getScopeValues(name, value) {
  if (value.type == 'object') {
    if (value.subtype == 'typedarray') return value.description;
    if (name == 'instance') return dumpInstanceProperties(value);
    if (name == 'function tables') return dumpTables(value);

    let msg = await Protocol.Runtime.getProperties({objectId: value.objectId});
    printIfFailure(msg);
    const printProperty = function(elem) {
      const wasmValue = WasmInspectorTest.getWasmValue(elem.value);
      return `"${elem.name}": ${wasmValue} (${elem.value.subtype})`;
    }
    return msg.result.result.map(printProperty).join(', ');
  }
  return WasmInspectorTest.getWasmValue(value) + ' (' + value.subtype + ')';
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

async function dumpTables(tablesObj) {
  let msg = await Protocol.Runtime.getProperties({objectId: tablesObj.objectId});
  var tables_str = [];
  for (var table of msg.result.result) {
    const func_entries = await recursiveGetPropertiesWrapper(table, 2);
    var functions = [];
    for (var func of func_entries) {
      for (var value of func.result.result) {
        functions.push(`${value.name}: ${value.value.description}`);
      }
    }
    const functions_str = functions.join(', ');
    tables_str.push(`      ${table.name}: ${functions_str}`);
  }
  return '\n' + tables_str.join('\n');
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
