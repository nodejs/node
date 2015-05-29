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
 * Accepts a CIF buffer and a JavaScript function,
 * and returns a C function pointer Buffer instance.
 */

exports.Closure = function Closure(cif, fn) {
  var fnPtr = binding.closureAlloc(cif, fn);

  // ensure that cif is not GC'd until after fnPtr is
  fnPtr.cif = cif;

  return fnPtr;
};
