'use strict';

const binding = process.binding('dlopen');

exports.dlopen = function dlopen(name) {
  var lib = new Buffer(binding.sizeof_uv_lib_t);

  if (binding.dlopen(name, lib) !== 0) {
    // error
    throw new Error(exports.dlerror(lib));
  }

  return lib;
};

exports.dlclose = function dlclose(lib) {
  return binding.dlclose(lib);
};

exports.dlsym = function dlsym(lib, name) {
  // TODO: use `sizeof.pointer` for buffer size when nodejs/io.js#1759 is merged
  var sym = new Buffer(8);

  if (binding.dlsym(lib, name, sym) !== 0) {
    // error
    throw new Error(exports.dlerror(lib));
  }

  return sym;
};

exports.dlerror = function dlerror(lib) {
  return binding.dlerror(lib);
};
