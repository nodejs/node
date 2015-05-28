'use strict';

const binding = process.binding('ffi');
const endianness = require('os').endianness();

exports = module.exports = binding;

/**
 * CIF interface describing a foreign function call.
 */

exports.CIF = function(abi, rType /* , aTypes... */) {
  var cif = new Buffer(binding.sizeof.ffi_cif);
  var numArgs = arguments.length - 1;

  if (typeof abi === 'number') {
    numArgs--;
  } else {
    rType = abi;
    abi = binding.FFI_DEFAULT_ABI;
  }

  var offset = 0;
  var aTypes = new Buffer(numArgs * binding.sizeof.pointer);
  for (var i = arguments.length - numArgs; i < arguments.length; i++) {
    aTypes['writePointer' + endianness](
        arguments[i], offset++ * binding.sizeof.pointer);
  }

  var status = binding.prepCif(cif, abi, numArgs, rType, aTypes);

  if (binding.FFI_OK !== status)
    throw new Error('prepCif failed with error: ' + status);

  return cif;
};

/**
 * Accepts a CIF buffer, a JavaScript function, and an optional userData
 * buffer pointer, and returns a C function pointer Buffer instance.
 */

exports.Closure = function Closure(cif, fn, userData) {
  var fnPtr = new Buffer(binding.sizeof.pointer);
  var closurePtr = binding.closureAlloc(fnPtr);

  if (!closurePtr)
    throw new Error('closureAlloc memory allocation failed');

  fnPtr = fnPtr['readPointer' + endianness](0);

  var status = binding.closurePrep(closurePtr, cif, fnPtr);

  if (binding.FFI_OK !== status)
    throw new Error('closurePrep failed with error: ' + status);

  // attach closurePtr to fnPtr, to prevent garbage collection of the former
  fnPtr.closure = closurePtr;

  return fnPtr;
};
