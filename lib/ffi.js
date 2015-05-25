const binding = process.binding('ffi');
const endianness = require('os').endianness();

exports = module.exports = binding;

/**
 * Foreign function. Turns a C function pointer into a JavaScript function.
 */

exports.Function = function Function(funcPointer, abi, rType /* , aTypes... */) {
  var cif = new Buffer(binding.sizeof.ffi_cif);
  var numArgs = arguments.length - 2;

  if (typeof abi === 'number') {
    numArgs--;
  } else {
    abi = binding.FFI_DEFAULT_ABI;
  }

  var aTypes = new Buffer(numArgs * binding.sizeof.pointer);
  for (var i = arguments.length - numArgs; i < arguments.length; i++) {
    aTypes['writePointer' + endianness](arguments[i], i * binding.sizeof.pointer);
  }

  var status = binding.prepCif(cif, numArgs, rType, aTypes, abi);
  console.log('status: %j', status);

  var fn = function() {
  };

  return fn;
};

/**
 * Callback function. Turns a JavaScript function into a C function pointer,
 * which may be passed to other FFI functions as arguments.
 */

exports.Callback = function Callback() {

};
