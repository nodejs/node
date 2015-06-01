'use strict';

const binding = process.binding('ffi');
const endianness = require('os').endianness();

exports.sizeof = binding.sizeof;
exports.alignof = binding.alignof;
exports.abi = binding.abi;
exports.types = binding.types;


function FFIError(name, status) {
  var err;
  switch (status) {
    case binding.FFI_BAD_TYPEDEF:
      err = new Error(name + ': FFI_BAD_TYPEDEF');
      err.code = 'FFI_BAD_TYPEDEF';
      break;
    case binding.FFI_BAD_ABI:
      err = new Error(name + ': FFI_BAD_ABI');
      err.code = 'FFI_BAD_ABI';
      break;
    default:
      err = new Error(name + ': ' + status);
      break;
  }
  err.errno = status;
  return err;
}


/**
 * CIF interface describing a foreign function call or closure.
 */

exports.CIF = function CIF(abi, rType /* , aTypes... */) {
  var cif = new Buffer(binding.sizeof.ffi_cif);
  var numArgs = arguments.length - 1;

  if (typeof abi === 'number') {
    numArgs--;
  } else {
    rType = abi;
    abi = binding.abi.default;
  }

  var offset = 0;
  var aTypes = new Buffer(numArgs * binding.sizeof.pointer);
  for (var i = arguments.length - numArgs; i < arguments.length; i++) {
    aTypes['writePointer' + endianness](
        arguments[i], offset++ * binding.sizeof.pointer);
  }

  var status = binding.prepCif(cif, abi, numArgs, rType, aTypes);

  if (binding.FFI_OK !== status)
    throw FFIError('ffi_prep_cif', status);

  return cif;
};


/**
 * Accepts a CIF buffer and a JavaScript function,
 * and returns a C function pointer Buffer instance.
 */

exports.Closure = function Closure(cif, fn) {
  var fnPtr = binding.closureAlloc(cif, fn);

  if (!fnPtr)
    throw new Error('ffi_closure_alloc memory allocation failed');
  else if (typeof fnPtr === 'number')
    throw FFIError('ffi_closure_alloc', fnPtr);
  // otherwise assume a Buffer instance was returned

  // ensure that cif is not GC'd until after fnPtr is
  fnPtr.cif = cif;

  return fnPtr;
};


/**
 * Invokes a C function pointer synchronously.
 */

exports.call = binding.call;
