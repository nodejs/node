// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/mjsunit/wasm/wasm-module-builder.js');

WasmInspectorTest = {}
InspectorTest.getWasmOpcodeName = getOpcodeName;

WasmInspectorTest.evalWithUrl = async function(code, url) {
  return await Protocol.Runtime
      .evaluate({'expression': code + '\n//# sourceURL=v8://test/' + url})
      .then(printIfFailure);
};

WasmInspectorTest.compileFromBuffer = (function(bytes) {
  var buffer = new ArrayBuffer(bytes.length);
  var view = new Uint8Array(buffer);
  for (var i = 0; i < bytes.length; ++i) {
    view[i] = bytes[i] | 0;
  }
  return new WebAssembly.Module(buffer);
}).toString();

WasmInspectorTest.instantiateFromBuffer =
    (function(bytes, imports) {
      return new WebAssembly.Instance(compileFromBuffer(bytes), imports);
    })
        .toString()
        .replace('compileFromBuffer', WasmInspectorTest.compileFromBuffer);

WasmInspectorTest.compile = async function(bytes, module_name = 'module') {
  const compile_code = `var ${module_name} = (${
      WasmInspectorTest.compileFromBuffer})(${JSON.stringify(bytes)});`;
  await WasmInspectorTest.evalWithUrl(compile_code, 'compile_module');
};

WasmInspectorTest.instantiate =
    async function(bytes, instance_name = 'instance') {
  const instantiate_code = `var ${instance_name} = (${
      WasmInspectorTest.instantiateFromBuffer})(${JSON.stringify(bytes)});`;
  await WasmInspectorTest.evalWithUrl(instantiate_code, 'instantiate');
};

WasmInspectorTest.dumpScopeProperties = async function(message) {
  printIfFailure(message);
  for (var value of message.result.result) {
    var value_str = await getScopeValues(value.name, value.value);
    InspectorTest.log('   ' + value.name + ': ' + value_str);
  }
};

WasmInspectorTest.getWasmValue = async function(value) {
  let msg = await Protocol.Runtime.getProperties({objectId: value.objectId});
  printIfFailure(msg);
  const value_type = msg.result.result.find(({name}) => name === 'type');
  const value_value = msg.result.result.find(({name}) => name === 'value');
  return `${
      value_value.value.unserializableValue ??
      value_value.value.value} (${value_type.value.value})`;
};

function printIfFailure(message) {
  if (!message.result) {
    InspectorTest.logMessage(message);
  }
  return message;
}

async function getScopeValues(name, value) {
  async function printValue(value) {
    if (value.type === 'object' && value.subtype === 'wasmvalue') {
      return await WasmInspectorTest.getWasmValue(value);
    } else if ('className' in value) {
      return `(${value.className})`;
    }
    return `${value.unserializableValue ?? value.value} (${
        value.subtype ?? value.type})`;
  }
  if (value.type === 'object' && value.subtype !== 'wasmvalue') {
    if (value.subtype === 'typedarray' || value.subtype == 'webassemblymemory')
      return value.description;
    if (name === 'instance') return dumpInstanceProperties(value);
    if (name === 'module') return value.description;

    let msg = await Protocol.Runtime.getProperties({objectId: value.objectId});
    printIfFailure(msg);
    async function printProperty({name, value}) {
      return `"${name}": ${await printValue(value)}`;
    }
    return (await Promise.all(msg.result.result.map(printProperty))).join(', ');
  }
  return await printValue(value);
}

function recursiveGetPropertiesWrapper(value, depth) {
  return recursiveGetProperties({result: {result: [value]}}, depth);
}

async function recursiveGetProperties(value, depth) {
  if (depth > 0) {
    const properties = await Promise.all(value.result.result.map(x => {
      return Protocol.Runtime.getProperties({objectId: x.value.objectId});
    }));
    const recursiveProperties = await Promise.all(properties.map(x => {
      return recursiveGetProperties(x, depth - 1);
    }));
    return recursiveProperties.flat();
  }
  return value;
}

async function dumpInstanceProperties(instanceObj) {
  function invokeGetter(property) {
    return this[JSON.parse(property)];
  }

  const exportsName = 'exports';
  let exportsObj = await Protocol.Runtime.callFunctionOn({
    objectId: instanceObj.objectId,
    functionDeclaration: invokeGetter.toString(),
    arguments: [{value: JSON.stringify(exportsName)}]
  });
  printIfFailure(exportsObj);
  let exports = await Protocol.Runtime.getProperties(
      {objectId: exportsObj.result.result.objectId});
  printIfFailure(exports);

  function printExports(value) {
    return `"${value.name}" (${value.value.className})`;
  }
  const formattedExports = exports.result.result.map(printExports).join(', ');
  return `${exportsName}: ${formattedExports}`
}
