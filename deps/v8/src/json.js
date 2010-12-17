// Copyright 2009 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

var $JSON = global.JSON;

function ParseJSONUnfiltered(text) {
  var s = $String(text);
  return %ParseJson(s);
}

function Revive(holder, name, reviver) {
  var val = holder[name];
  if (IS_OBJECT(val)) {
    if (IS_ARRAY(val)) {
      var length = val.length;
      for (var i = 0; i < length; i++) {
        var newElement = Revive(val, $String(i), reviver);
        val[i] = newElement;
      }
    } else {
      for (var p in val) {
        if (ObjectHasOwnProperty.call(val, p)) {
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
  return reviver.call(holder, name, val);
}

function JSONParse(text, reviver) {
  var unfiltered = ParseJSONUnfiltered(text);
  if (IS_FUNCTION(reviver)) {
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
  var partial = [];
  var len = value.length;
  for (var i = 0; i < len; i++) {
    var strP = JSONSerialize($String(i), value, replacer, stack,
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
  var partial = [];
  if (IS_ARRAY(replacer)) {
    var length = replacer.length;
    for (var i = 0; i < length; i++) {
      if (ObjectHasOwnProperty.call(replacer, i)) {
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
      if (ObjectHasOwnProperty.call(value, p)) {
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
    if (IS_FUNCTION(toJSON)) {
      value = %_CallFunction(value, key, toJSON);
    }
  }
  if (IS_FUNCTION(replacer)) {
    value = %_CallFunction(holder, key, value, replacer);
  }
  if (IS_STRING(value)) {
    return %QuoteJSONString(value);
  } else if (IS_NUMBER(value)) {
    return $isFinite(value) ? $String(value) : "null";
  } else if (IS_BOOLEAN(value)) {
    return value ? "true" : "false";
  } else if (IS_NULL(value)) {
    return "null";
  } else if (IS_SPEC_OBJECT(value) && !(typeof value == "function")) {
    // Non-callable object. If it's a primitive wrapper, it must be unwrapped.
    if (IS_ARRAY(value)) {
      return SerializeArray(value, replacer, stack, indent, gap);
    } else if (IS_NUMBER_WRAPPER(value)) {
      value = ToNumber(value);
      return $isFinite(value) ? ToString(value) : "null";
    } else if (IS_STRING_WRAPPER(value)) {
      return %QuoteJSONString(ToString(value));
    } else if (IS_BOOLEAN_WRAPPER(value)) {
      return %_ValueOf(value) ? "true" : "false";
    } else {
      return SerializeObject(value, replacer, stack, indent, gap);
    }
  }
  // Undefined or a callable object.
  return void 0;
}


function BasicSerializeArray(value, stack, builder) {
  if (!%PushIfAbsent(stack, value)) {
    throw MakeTypeError('circular_structure', []);
  }
  builder.push("[");
  var len = value.length;
  for (var i = 0; i < len; i++) {
    var before = builder.length;
    BasicJSONSerialize(i, value, stack, builder);
    if (before == builder.length) builder.push("null");
    builder.push(",");
  }
  stack.pop();
  if (builder.pop() != ",") {
    builder.push("[]");  // Zero length array. Push "[" back on.
  } else {
    builder.push("]");
  }

}


function BasicSerializeObject(value, stack, builder) {
  if (!%PushIfAbsent(stack, value)) {
    throw MakeTypeError('circular_structure', []);
  }
  builder.push("{");
  for (var p in value) {
    if (%HasLocalProperty(value, p)) {
      builder.push(%QuoteJSONString(p));
      builder.push(":");
      var before = builder.length;
      BasicJSONSerialize(p, value, stack, builder);
      if (before == builder.length) {
        builder.pop();
        builder.pop();
      } else {
        builder.push(",");
      }
    }
  }
  stack.pop();
  if (builder.pop() != ",") {
    builder.push("{}");  // Object has no own properties. Push "{" back on.
  } else {
    builder.push("}");
  }
}


function BasicJSONSerialize(key, holder, stack, builder) {
  var value = holder[key];
  if (IS_SPEC_OBJECT(value)) {
    var toJSON = value.toJSON;
    if (IS_FUNCTION(toJSON)) {
      value = %_CallFunction(value, ToString(key), toJSON);
    }
  }
  if (IS_STRING(value)) {
    builder.push(%QuoteJSONString(value));
  } else if (IS_NUMBER(value)) {
    builder.push(($isFinite(value) ? %_NumberToString(value) : "null"));
  } else if (IS_BOOLEAN(value)) {
    builder.push(value ? "true" : "false");
  } else if (IS_NULL(value)) {
    builder.push("null");
  } else if (IS_SPEC_OBJECT(value) && !(typeof value == "function")) {
    // Value is a non-callable object.
    // Unwrap value if necessary
    if (IS_NUMBER_WRAPPER(value)) {
      value = ToNumber(value);
      builder.push(($isFinite(value) ? %_NumberToString(value) : "null"));
    } else if (IS_STRING_WRAPPER(value)) {
      builder.push(%QuoteJSONString(ToString(value)));
    } else if (IS_BOOLEAN_WRAPPER(value)) {
      builder.push(%_ValueOf(value) ? "true" : "false");
    } else if (IS_ARRAY(value)) {
      BasicSerializeArray(value, stack, builder);
    } else {
      BasicSerializeObject(value, stack, builder);
    }
  }
}


function JSONStringify(value, replacer, space) {
  if (%_ArgumentsLength() == 1) {
    var builder = [];
    BasicJSONSerialize('', {'': value}, [], builder);
    if (builder.length == 0) return;
    var result = %_FastAsciiArrayJoin(builder, "");
    if (!IS_UNDEFINED(result)) return result;
    return %StringBuilderConcat(builder, builder.length, "");
  }
  if (IS_OBJECT(space)) {
    // Unwrap 'space' if it is wrapped
    if (IS_NUMBER_WRAPPER(space)) {
      space = ToNumber(space);
    } else if (IS_STRING_WRAPPER(space)) {
      space = ToString(space);
    }
  }
  var gap;
  if (IS_NUMBER(space)) {
    space = MathMax(0, MathMin(ToInteger(space), 10));
    gap = SubString("          ", 0, space);
  } else if (IS_STRING(space)) {
    if (space.length > 10) {
      gap = SubString(space, 0, 10);
    } else {
      gap = space;
    }
  } else {
    gap = "";
  }
  return JSONSerialize('', {'': value}, replacer, [], "", gap);
}

function SetupJSON() {
  InstallFunctions($JSON, DONT_ENUM, $Array(
    "parse", JSONParse,
    "stringify", JSONStringify
  ));
}

SetupJSON();
