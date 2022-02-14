"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = applyDecs;

function createMetadataMethodsForProperty(metadataMap, kind, property) {
  return {
    getMetadata: function (key) {
      if (typeof key !== "symbol") {
        throw new TypeError("Metadata keys must be symbols, received: " + key);
      }

      var metadataForKey = metadataMap[key];
      if (metadataForKey === void 0) return void 0;

      if (kind === 1) {
        var pub = metadataForKey.public;

        if (pub !== void 0) {
          return pub[property];
        }
      } else if (kind === 2) {
        var priv = metadataForKey.private;

        if (priv !== void 0) {
          return priv.get(property);
        }
      } else if (Object.hasOwnProperty.call(metadataForKey, "constructor")) {
        return metadataForKey.constructor;
      }
    },
    setMetadata: function (key, value) {
      if (typeof key !== "symbol") {
        throw new TypeError("Metadata keys must be symbols, received: " + key);
      }

      var metadataForKey = metadataMap[key];

      if (metadataForKey === void 0) {
        metadataForKey = metadataMap[key] = {};
      }

      if (kind === 1) {
        var pub = metadataForKey.public;

        if (pub === void 0) {
          pub = metadataForKey.public = {};
        }

        pub[property] = value;
      } else if (kind === 2) {
        var priv = metadataForKey.priv;

        if (priv === void 0) {
          priv = metadataForKey.private = new Map();
        }

        priv.set(property, value);
      } else {
        metadataForKey.constructor = value;
      }
    }
  };
}

function convertMetadataMapToFinal(obj, metadataMap) {
  var parentMetadataMap = obj[Symbol.metadata || Symbol.for("Symbol.metadata")];
  var metadataKeys = Object.getOwnPropertySymbols(metadataMap);
  if (metadataKeys.length === 0) return;

  for (var i = 0; i < metadataKeys.length; i++) {
    var key = metadataKeys[i];
    var metaForKey = metadataMap[key];
    var parentMetaForKey = parentMetadataMap ? parentMetadataMap[key] : null;
    var pub = metaForKey.public;
    var parentPub = parentMetaForKey ? parentMetaForKey.public : null;

    if (pub && parentPub) {
      Object.setPrototypeOf(pub, parentPub);
    }

    var priv = metaForKey.private;

    if (priv) {
      var privArr = Array.from(priv.values());
      var parentPriv = parentMetaForKey ? parentMetaForKey.private : null;

      if (parentPriv) {
        privArr = privArr.concat(parentPriv);
      }

      metaForKey.private = privArr;
    }

    if (parentMetaForKey) {
      Object.setPrototypeOf(metaForKey, parentMetaForKey);
    }
  }

  if (parentMetadataMap) {
    Object.setPrototypeOf(metadataMap, parentMetadataMap);
  }

  obj[Symbol.metadata || Symbol.for("Symbol.metadata")] = metadataMap;
}

function createAddInitializerMethod(initializers) {
  return function addInitializer(initializer) {
    assertValidInitializer(initializer);
    initializers.push(initializer);
  };
}

function memberDecCtx(base, name, desc, metadataMap, initializers, kind, isStatic, isPrivate) {
  var kindStr;

  switch (kind) {
    case 1:
      kindStr = "accessor";
      break;

    case 2:
      kindStr = "method";
      break;

    case 3:
      kindStr = "getter";
      break;

    case 4:
      kindStr = "setter";
      break;

    default:
      kindStr = "field";
  }

  var ctx = {
    kind: kindStr,
    name: isPrivate ? "#" + name : name,
    isStatic: isStatic,
    isPrivate: isPrivate
  };

  if (kind !== 0) {
    ctx.addInitializer = createAddInitializerMethod(initializers);
  }

  var metadataKind, metadataName;

  if (isPrivate) {
    metadataKind = 2;
    metadataName = Symbol(name);
    var access = {};

    if (kind === 0) {
      access.get = desc.get;
      access.set = desc.set;
    } else if (kind === 2) {
      access.get = function () {
        return desc.value;
      };
    } else {
      if (kind === 1 || kind === 3) {
        access.get = function () {
          return desc.get.call(this);
        };
      }

      if (kind === 1 || kind === 4) {
        access.set = function (v) {
          desc.set.call(this, v);
        };
      }
    }

    ctx.access = access;
  } else {
    metadataKind = 1;
    metadataName = name;
  }

  return Object.assign(ctx, createMetadataMethodsForProperty(metadataMap, metadataKind, metadataName));
}

function assertValidInitializer(initializer) {
  if (typeof initializer !== "function") {
    throw new Error("initializers must be functions");
  }
}

function assertValidReturnValue(kind, value) {
  var type = typeof value;

  if (kind === 1) {
    if (type !== "object" || value === null) {
      throw new Error("accessor decorators must return an object with get, set, or initializer properties or void 0");
    }
  } else if (type !== "function") {
    if (kind === 0) {
      throw new Error("field decorators must return a initializer function or void 0");
    } else {
      throw new Error("method decorators must return a function or void 0");
    }
  }
}

function applyMemberDec(ret, base, decInfo, name, kind, isStatic, isPrivate, metadataMap, initializers) {
  var decs = decInfo[0];
  var desc, initializer, value;

  if (isPrivate) {
    if (kind === 0 || kind === 1) {
      desc = {
        get: decInfo[3],
        set: decInfo[4]
      };
    } else if (kind === 3) {
      desc = {
        get: decInfo[3]
      };
    } else if (kind === 4) {
      desc = {
        set: decInfo[3]
      };
    } else {
      desc = {
        value: decInfo[3]
      };
    }
  } else if (kind !== 0) {
    desc = Object.getOwnPropertyDescriptor(base, name);
  }

  if (kind === 1) {
    value = {
      get: desc.get,
      set: desc.set
    };
  } else if (kind === 2) {
    value = desc.value;
  } else if (kind === 3) {
    value = desc.get;
  } else if (kind === 4) {
    value = desc.set;
  }

  var ctx = memberDecCtx(base, name, desc, metadataMap, initializers, kind, isStatic, isPrivate);
  var newValue, get, set;

  if (typeof decs === "function") {
    newValue = decs(value, ctx);

    if (newValue !== void 0) {
      assertValidReturnValue(kind, newValue);

      if (kind === 0) {
        initializer = newValue;
      } else if (kind === 1) {
        initializer = newValue.initializer;
        get = newValue.get || value.get;
        set = newValue.set || value.set;
        value = {
          get: get,
          set: set
        };
      } else {
        value = newValue;
      }
    }
  } else {
    for (var i = decs.length - 1; i >= 0; i--) {
      var dec = decs[i];
      newValue = dec(value, ctx);

      if (newValue !== void 0) {
        assertValidReturnValue(kind, newValue);
        var newInit;

        if (kind === 0) {
          newInit = newValue;
        } else if (kind === 1) {
          newInit = newValue.initializer;
          get = newValue.get || value.get;
          set = newValue.set || value.set;
          value = {
            get: get,
            set: set
          };
        } else {
          value = newValue;
        }

        if (newInit !== void 0) {
          if (initializer === void 0) {
            initializer = newInit;
          } else if (typeof initializer === "function") {
            initializer = [initializer, newInit];
          } else {
            initializer.push(newInit);
          }
        }
      }
    }
  }

  if (kind === 0 || kind === 1) {
    if (initializer === void 0) {
      initializer = function (instance, init) {
        return init;
      };
    } else if (typeof initializer !== "function") {
      var ownInitializers = initializer;

      initializer = function (instance, init) {
        var value = init;

        for (var i = 0; i < ownInitializers.length; i++) {
          value = ownInitializers[i].call(instance, value);
        }

        return value;
      };
    } else {
      var originalInitializer = initializer;

      initializer = function (instance, init) {
        return originalInitializer.call(instance, init);
      };
    }

    ret.push(initializer);
  }

  if (kind !== 0) {
    if (kind === 1) {
      desc.get = value.get;
      desc.set = value.set;
    } else if (kind === 2) {
      desc.value = value;
    } else if (kind === 3) {
      desc.get = value;
    } else if (kind === 4) {
      desc.set = value;
    }

    if (isPrivate) {
      if (kind === 1) {
        ret.push(function (instance, args) {
          return value.get.call(instance, args);
        });
        ret.push(function (instance, args) {
          return value.set.call(instance, args);
        });
      } else if (kind === 2) {
        ret.push(value);
      } else {
        ret.push(function (instance, args) {
          return value.call(instance, args);
        });
      }
    } else {
      Object.defineProperty(base, name, desc);
    }
  }
}

function applyMemberDecs(ret, Class, protoMetadataMap, staticMetadataMap, decInfos) {
  var protoInitializers = [];
  var staticInitializers = [];
  var existingProtoNonFields = new Map();
  var existingStaticNonFields = new Map();

  for (var i = 0; i < decInfos.length; i++) {
    var decInfo = decInfos[i];
    if (!Array.isArray(decInfo)) continue;
    var kind = decInfo[1];
    var name = decInfo[2];
    var isPrivate = decInfo.length > 3;
    var isStatic = kind >= 5;
    var base;
    var metadataMap;
    var initializers;

    if (isStatic) {
      base = Class;
      metadataMap = staticMetadataMap;
      kind = kind - 5;
      initializers = staticInitializers;
    } else {
      base = Class.prototype;
      metadataMap = protoMetadataMap;
      initializers = protoInitializers;
    }

    if (kind !== 0 && !isPrivate) {
      var existingNonFields = isStatic ? existingStaticNonFields : existingProtoNonFields;
      var existingKind = existingNonFields.get(name) || 0;

      if (existingKind === true || existingKind === 3 && kind !== 4 || existingKind === 4 && kind !== 3) {
        throw new Error("Attempted to decorate a public method/accessor that has the same name as a previously decorated public method/accessor. This is not currently supported by the decorators plugin. Property name was: " + name);
      } else if (!existingKind && kind > 2) {
        existingNonFields.set(name, kind);
      } else {
        existingNonFields.set(name, true);
      }
    }

    applyMemberDec(ret, base, decInfo, name, kind, isStatic, isPrivate, metadataMap, initializers);
  }

  if (protoInitializers.length > 0) {
    pushInitializers(ret, protoInitializers);
  }

  if (staticInitializers.length > 0) {
    pushInitializers(ret, staticInitializers);
  }
}

function pushInitializers(ret, initializers) {
  if (initializers.length > 0) {
    initializers = initializers.slice();
    ret.push(function (instance) {
      for (var i = 0; i < initializers.length; i++) {
        initializers[i].call(instance, instance);
      }

      return instance;
    });
  } else {
    ret.push(function (instance) {
      return instance;
    });
  }
}

function applyClassDecs(ret, targetClass, metadataMap, classDecs) {
  var initializers = [];
  var newClass = targetClass;
  var name = targetClass.name;
  var ctx = Object.assign({
    kind: "class",
    name: name,
    addInitializer: createAddInitializerMethod(initializers)
  }, createMetadataMethodsForProperty(metadataMap, 0, name));

  for (var i = classDecs.length - 1; i >= 0; i--) {
    newClass = classDecs[i](newClass, ctx) || newClass;
  }

  ret.push(newClass);

  if (initializers.length > 0) {
    ret.push(function () {
      for (var i = 0; i < initializers.length; i++) {
        initializers[i].call(newClass, newClass);
      }
    });
  } else {
    ret.push(function () {});
  }
}

function applyDecs(targetClass, memberDecs, classDecs) {
  var ret = [];
  var staticMetadataMap = {};

  if (memberDecs) {
    var protoMetadataMap = {};
    applyMemberDecs(ret, targetClass, protoMetadataMap, staticMetadataMap, memberDecs);
    convertMetadataMapToFinal(targetClass.prototype, protoMetadataMap);
  }

  if (classDecs) {
    applyClassDecs(ret, targetClass, staticMetadataMap, classDecs);
  }

  convertMetadataMapToFinal(targetClass, staticMetadataMap);
  return ret;
}