"use strict";

(() => {
  function write(ast, opt = {}) {
    const noop = str => str;
    const optNames = "type".split(" ");
    const context = [];
    for (const o of optNames) {
      if (!opt[o]) opt[o] = noop;
    }

    function literal(it) {
      return it.value;
    };
    function type(it) {
      if (typeof it === "string") return opt.type(it); // XXX should maintain some context
      let ret = extended_attributes(it.extAttrs);
      if (it.union) ret += `(${it.idlType.map(type).join(" or ")})`;
      else {
        if (it.generic) ret += `${it.generic}<`;
        if (Array.isArray(it.idlType)) ret += it.idlType.map(type).join(", ");
        else ret += type(it.idlType);
        if (it.generic) ret += ">";
      }
      if (it.nullable) ret += "?";

      return ret;
    };
    function const_value(it) {
      const tp = it.type;
      if (tp === "boolean") return it.value ? "true" : "false";
      else if (tp === "null") return "null";
      else if (tp === "Infinity") return (it.negative ? "-" : "") + "Infinity";
      else if (tp === "NaN") return "NaN";
      else if (tp === "number") return it.value;
      else if (tp === "sequence") return "[]";
      else return `"${it.value}"`;
    };
    function argument(arg) {
      let ret = extended_attributes(arg.extAttrs);
      if (arg.optional) ret += "optional ";
      ret += type(arg.idlType);
      if (arg.variadic) ret += "...";
      ret += ` ${arg.escapedName}`;
      if (arg.default) ret += ` = ${const_value(arg.default)}`;
      return ret;
    };
    function make_ext_at(it) {
      context.unshift(it);
      let ret = it.name;
      if (it.rhs) {
        if (it.rhs.type === "identifier-list") ret += `=(${it.rhs.value.join(",")})`;
        else ret += `=${it.rhs.value}`;
      }
      if (it.arguments) ret += `(${it.arguments.length ? it.arguments.map(argument).join(",") : ""})`;
      context.shift(); // XXX need to add more contexts, but not more than needed for ReSpec
      return ret;
    };
    function extended_attributes(eats) {
      if (!eats || !eats.length) return "";
      return `[${eats.map(make_ext_at).join(", ")}]`;
    };

    const modifiers = "getter setter deleter stringifier static".split(" ");
    function operation(it) {
      let ret = extended_attributes(it.extAttrs);
      if (it.stringifier && !it.idlType) return "stringifier;";
      for (const mod of modifiers) {
        if (it[mod]) ret += mod + " ";
      }
      ret += type(it.idlType) + " ";
      if (it.name) ret += it.escapedName;
      ret += `(${it.arguments.map(argument).join(",")});`;
      return ret;
    };

    function attribute(it) {
      let ret = extended_attributes(it.extAttrs);
      if (it.static) ret += "static ";
      if (it.stringifier) ret += "stringifier ";
      if (it.inherit) ret += "inherit ";
      if (it.readonly) ret += "readonly ";
      ret += `attribute ${type(it.idlType)} ${it.escapedName};`;
      return ret;
    };

    function interface_(it) {
      let ret = extended_attributes(it.extAttrs);
      if (it.partial) ret += "partial ";
      ret += `interface ${it.name} `;
      if (it.inheritance) ret += `: ${it.inheritance} `;
      ret += `{${iterate(it.members)}};`;
      return ret;
    };

    function interface_mixin(it) {
      let ret = extended_attributes(it.extAttrs);
      if (it.partial) ret += "partial ";
      ret += `interface mixin ${it.name} `;
      ret += `{${iterate(it.members)}};`;
      return ret;
    }

    function namespace(it) {
      let ret = extended_attributes(it.extAttrs);
      if (it.partial) ret += "partial ";
      ret += `namespace ${it.name} `;
      ret += `{${iterate(it.members)}};`;
      return ret;
    }

    function dictionary(it) {
      let ret = extended_attributes(it.extAttrs);
      if (it.partial) ret += "partial ";
      ret += `dictionary ${it.name} `;
      if (it.inheritance) ret += `: ${it.inheritance} `;
      ret += `{${iterate(it.members)}};`;
      return ret;
    };
    function field(it) {
      let ret = extended_attributes(it.extAttrs);
      if (it.required) ret += "required ";
      ret += `${type(it.idlType)} ${it.escapedName}`;
      if (it.default) ret += ` = ${const_value(it.default)}`;
      ret += ";";
      return ret;
    };
    function const_(it) {
      const ret = extended_attributes(it.extAttrs);
      return `${ret}const ${type(it.idlType)}${it.nullable ? "?" : ""} ${it.name} = ${const_value(it.value)};`;
    };
    function typedef(it) {
      let ret = extended_attributes(it.extAttrs);
      ret += `typedef ${extended_attributes(it.typeExtAttrs)}`;
      return `${ret}${type(it.idlType)} ${it.name};`;
    };
    function implements_(it) {
      const ret = extended_attributes(it.extAttrs);
      return `${ret}${it.target} implements ${it.implements};`;
    };
    function includes(it) {
      const ret = extended_attributes(it.extAttrs);
      return `${ret}${it.target} includes ${it.includes};`;
    };
    function callback(it) {
      const ret = extended_attributes(it.extAttrs);
      return `${ret}callback ${it.name} = ${type(it.idlType)}(${it.arguments.map(argument).join(",")});`;
    };
    function enum_(it) {
      let ret = extended_attributes(it.extAttrs);
      ret += `enum ${it.name} {`;
      for (const v of it.values) {
        ret += `"${v.value}",`;
      }
      return ret + "};";
    };
    function iterable(it) {
      return `iterable<${Array.isArray(it.idlType) ? it.idlType.map(type).join(", ") : type(it.idlType)}>;`;
    };
    function legacyiterable(it) {
      return `legacyiterable<${Array.isArray(it.idlType) ? it.idlType.map(type).join(", ") : type(it.idlType)}>;`;
    };
    function maplike(it) {
      return `${it.readonly ? "readonly " : ""}maplike<${it.idlType.map(type).join(", ")}>;`;
    };
    function setlike(it) {
      return `${it.readonly ? "readonly " : ""}setlike<${type(it.idlType[0])}>;`;
    };
    function callbackInterface(it) {
      return `callback ${interface_(it)}`;
    };

    const table = {
      interface: interface_,
      "interface mixin": interface_mixin,
      namespace,
      operation,
      attribute,
      dictionary,
      field,
      const: const_,
      typedef,
      implements: implements_,
      includes,
      callback,
      enum: enum_,
      iterable,
      legacyiterable,
      maplike,
      setlike,
      "callback interface": callbackInterface
    };
    function dispatch(it) {
      const dispatcher = table[it.type];
      if (!dispatcher) {
        throw new Error(`Type "${it.type}" is unsupported`)
      }
      return table[it.type](it);
    };
    function iterate(things) {
      if (!things) return;
      let ret = "";
      for (const thing of things) ret += dispatch(thing);
      return ret;
    };
    return iterate(ast);
  };


  const obj = {
    write
  };

  if (typeof module !== 'undefined' && typeof module.exports !== 'undefined') {
    module.exports = obj;
  } else if (typeof define === 'function' && define.amd) {
    define([], () => obj);
  } else {
    (self || window).WebIDL2Writer = obj;
  }
})();
