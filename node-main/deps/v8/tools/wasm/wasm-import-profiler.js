// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(() => {
  let all_profiles = [];
  let instanceMap = new WeakMap();
  let instanceCounter = 0;

  function instrument(imports, profile) {
    let orig_imports = imports;
    return new Proxy(imports, {
      get: (obj, module_name) => {
        let orig_module = orig_imports[module_name];
        return new Proxy(orig_module, {
          get: (obj, item_name) => {
            let orig_func = orig_module[item_name];
            let item = orig_func;
            if (typeof orig_func == "function") {
              var full_name = module_name + "." + item_name;
              print("instrumented " + full_name);
              profile[full_name] = {name: full_name, count: 0, total: 0};
              item = function profiled_func(...args) {
                var before = performance.now();
                var result = orig_func(...args);
                var delta = performance.now() - before;
                var data = profile[full_name];
                data.count++;
                data.total += delta;
                return result;
              }
            }
            return item;
          }
        })
      }
    });
  }

  function dumpProfile(profile) {
    let array = [];
    for (let key in profile) {
      if (key == "instanceNum") continue;
      let data = profile[key];
      if (data.count == 0) continue;
      array.push(data);
    }
    print(`--- Import profile for instance ${profile.instanceNum} ---`);
    if (array.length == 0) return;
    array.sort((a, b) => b.total - a.total);
    for (let data of array) {
      print(`${padl(data.name, 30)}: ${padr(data.count, 10)} ${padp(data.total, 10)}ms`);
    }
  }

  function padl(s, len) {
    s = s.toString();
    while (s.length < len) s = s + " ";
    return s;
  }
  function padr(s, len) {
    s = s.toString();
    while (s.length < len) s = " " + s;
    return s;
  }
  function padp(s, len) {
    s = s.toString();
    var i = s.indexOf(".");
    if (i == -1) i = s.length;
    while (i++ < len) s = " " + s;
    return s;
  }

  // patch: WebAssembly.instantiate (async)
  let orig_instantiate = WebAssembly.instantiate;
  WebAssembly.instantiate = (m, imports, ...args) => {
    let profile = {};
    let promise = orig_instantiate(m, instrument(imports, profile), ...args);
    promise.then((instance) => {
      instanceMap.set(instance, profile);
      all_profiles.push(profile);
      profile.instanceNum = instanceCounter++;
    });
    return promise;
  }

  // patch: new WebAssembly.Instance (sync)
  let orig_new_instance = WebAssembly.Instance;
  WebAssembly.Instance = new Proxy(orig_new_instance, {
    construct: (target, args) => {
      let profile = {};
      args[1] = instrument(args[1], profile);
      let instance = new orig_new_instance(...args);
      instanceMap.set(instance, profile);
      all_profiles.push(profile);
      profile.instanceNum = instanceCounter++;
      return instance;
    }
  });

  // expose: WebAssembly.dumpProfile(instance)
  WebAssembly.dumpProfile = (instance) => {
    let profile = instanceMap.get(instance);
    if (profile === undefined) return;
    dumpProfile(profile);
  }
  // expose: WebAssembly.clearProfile(instance)
  WebAssembly.clearProfile = (instance) => {
    let profile = instanceMap.get(instance);
    if (profile === undefined) return;
    for (let key in profile) {
      if (key == "instanceNum") continue;
      let data = p[key];
      data.count = 0;
      data.total = 0;
    }
  }
  // expose: WebAssembly.dumpAllProfiles()
  WebAssembly.dumpAllProfiles = () => {
    for (let profile of all_profiles) dumpProfile(profile);
  }
  // expose: WebAssembly.getProfile(instance)
  // returns: {
  //    func_name1: {name: func_name1, count: <num>, total: <num>}
  //    func_name2: {name: func_name1, count: <num>, total: <num>}
  //    ...
  // }
  WebAssembly.getProfile = (instance) => {
    return instanceMap.get(instance);
  }
})();
