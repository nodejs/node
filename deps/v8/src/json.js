// Copyright 2009 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var $jsonSerializeAdapter;

(function(global, shared, exports) {

"use strict";

%CheckIsBootstrapping();

var GlobalJSON = global.JSON;

// -------------------------------------------------------------------

function Revive(holder, name, reviver) {
  var val = holder[name];
  if (IS_OBJECT(val)) {
    if (IS_ARRAY(val)) {
      var length = val.length;
      for (var i = 0; i < length; i++) {
        var newElement = Revive(val, %_NumberToString(i), reviver);
        val[i] = newElement;
      }
    } else {
      for (var p in val) {
        if (%_CallFunction(val, p, $objectHasOwnProperty)) {
          var newElement = Revive(val, p, reviver);
          if (IS_UNDEFINED(newElement)) {
            delete val[p];
          } else {
            val[p] = newElement;
          }
        }
      }
    }
  }
  return %_CallFunction(holder, name, val, reviver);
}


function JSONParse(text, reviver) {
  var unfiltered = %ParseJson(TO_STRING_INLINE(text));
  if (IS_SPEC_FUNCTION(reviver)) {
    return Revive({'': unfiltered}, '', reviver);
  } else {
    return unfiltered;
  }
}


function SerializeArray(value, replacer, stack, indent, gap) {
  if (!%PushIfAbsent(stack, value)) {
    throw MakeTypeError('circular_structure', []);
  }
  var stepback = indent;
  indent += gap;
  var partial = new InternalArray();
  var len = value.length;
  for (var i = 0; i < len; i++) {
    var strP = JSONSerialize(%_NumberToString(i), value, replacer, stack,
                             indent, gap);
    if (IS_UNDEFINED(strP)) {
      strP = "null";
    }
    partial.push(strP);
  }
  var final;
  if (gap == "") {
    final = "[" + partial.join(",") + "]";
  } else if (partial.length > 0) {
    var separator = ",\n" + indent;
    final = "[\n" + indent + partial.join(separator) + "\n" +
        stepback + "]";
  } else {
    final = "[]";
  }
  stack.pop();
  return final;
}


function SerializeObject(value, replacer, stack, indent, gap) {
  if (!%PushIfAbsent(stack, value)) {
    throw MakeTypeError('circular_structure', []);
  }
  var stepback = indent;
  indent += gap;
  var partial = new InternalArray();
  if (IS_ARRAY(replacer)) {
    var length = replacer.length;
    for (var i = 0; i < length; i++) {
      if (%_CallFunction(replacer, i, $objectHasOwnProperty)) {
        var p = replacer[i];
        var strP = JSONSerialize(p, value, replacer, stack, indent, gap);
        if (!IS_UNDEFINED(strP)) {
          var member = %QuoteJSONString(p) + ":";
          if (gap != "") member += " ";
          member += strP;
          partial.push(member);
        }
      }
    }
  } else {
    for (var p in value) {
      if (%_CallFunction(value, p, $objectHasOwnProperty)) {
        var strP = JSONSerialize(p, value, replacer, stack, indent, gap);
        if (!IS_UNDEFINED(strP)) {
          var member = %QuoteJSONString(p) + ":";
          if (gap != "") member += " ";
          member += strP;
          partial.push(member);
        }
      }
    }
  }
  var final;
  if (gap == "") {
    final = "{" + partial.join(",") + "}";
  } else if (partial.length > 0) {
    var separator = ",\n" + indent;
    final = "{\n" + indent + partial.join(separator) + "\n" +
        stepback + "}";
  } else {
    final = "{}";
  }
  stack.pop();
  return final;
}


function JSONSerialize(key, holder, replacer, stack, indent, gap) {
  var value = holder[key];
  if (IS_SPEC_OBJECT(value)) {
    var toJSON = value.toJSON;
    if (IS_SPEC_FUNCTION(toJSON)) {
      value = %_CallFunction(value, key, toJSON);
    }
  }
  if (IS_SPEC_FUNCTION(replacer)) {
    value = %_CallFunction(holder, key, value, replacer);
  }
  if (IS_STRING(value)) {
    return %QuoteJSONString(value);
  } else if (IS_NUMBER(value)) {
    return JSON_NUMBER_TO_STRING(value);
  } else if (IS_BOOLEAN(value)) {
    return value ? "true" : "false";
  } else if (IS_NULL(value)) {
    return "null";
  } else if (IS_SPEC_OBJECT(value) && !(typeof value == "function")) {
    // Non-callable object. If it's a primitive wrapper, it must be unwrapped.
    if (IS_ARRAY(value)) {
      return SerializeArray(value, replacer, stack, indent, gap);
    } else if (IS_NUMBER_WRAPPER(value)) {
      value = $toNumber(value);
      return JSON_NUMBER_TO_STRING(value);
    } else if (IS_STRING_WRAPPER(value)) {
      return %QuoteJSONString($toString(value));
    } else if (IS_BOOLEAN_WRAPPER(value)) {
      return %_ValueOf(value) ? "true" : "false";
    } else {
      return SerializeObject(value, replacer, stack, indent, gap);
    }
  }
  // Undefined or a callable object.
  return UNDEFINED;
}


function JSONStringify(value, replacer, space) {
  if (%_ArgumentsLength() == 1) {
    return %BasicJSONStringify(value);
  }
  if (IS_OBJECT(space)) {
    // Unwrap 'space' if it is wrapped
    if (IS_NUMBER_WRAPPER(space)) {
      space = $toNumber(space);
    } else if (IS_STRING_WRAPPER(space)) {
      space = $toString(space);
    }
  }
  var gap;
  if (IS_NUMBER(space)) {
    space = $max(0, $min($toInteger(space), 10));
    gap = %_SubString("          ", 0, space);
  } else if (IS_STRING(space)) {
    if (space.length > 10) {
      gap = %_SubString(space, 0, 10);
    } else {
      gap = space;
    }
  } else {
    gap = "";
  }
  if (IS_ARRAY(replacer)) {
    // Deduplicate replacer array items.
    var property_list = new InternalArray();
    var seen_properties = { __proto__: null };
    var seen_sentinel = {};
    var length = replacer.length;
    for (var i = 0; i < length; i++) {
      var item = replacer[i];
      if (IS_STRING_WRAPPER(item)) {
        item = $toString(item);
      } else {
        if (IS_NUMBER_WRAPPER(item)) item = $toNumber(item);
        if (IS_NUMBER(item)) item = %_NumberToString(item);
      }
      if (IS_STRING(item) && seen_properties[item] != seen_sentinel) {
        property_list.push(item);
        // We cannot use true here because __proto__ needs to be an object.
        seen_properties[item] = seen_sentinel;
      }
    }
    replacer = property_list;
  }
  return JSONSerialize('', {'': value}, replacer, new InternalArray(), "", gap);
}

// -------------------------------------------------------------------

%AddNamedProperty(GlobalJSON, symbolToStringTag, "JSON", READ_ONLY | DONT_ENUM);

// Set up non-enumerable properties of the JSON object.
$installFunctions(GlobalJSON, DONT_ENUM, [
  "parse", JSONParse,
  "stringify", JSONStringify
]);

// -------------------------------------------------------------------
// JSON Builtins

$jsonSerializeAdapter = function(key, object) {
  var holder = {};
  holder[key] = object;
  // No need to pass the actual holder since there is no replacer function.
  return JSONSerialize(key, holder, UNDEFINED, new InternalArray(), "", "");
}

})
