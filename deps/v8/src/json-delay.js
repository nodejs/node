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
  var f = %CompileString(text, true);
  return f();
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

var characterQuoteCache = {
  '\"': '\\"',
  '\\': '\\\\',
  '/': '\\/',
  '\b': '\\b',
  '\f': '\\f',
  '\n': '\\n',
  '\r': '\\r',
  '\t': '\\t',
  '\x0B': '\\u000b'
};

function QuoteSingleJSONCharacter(c) {
  if (c in characterQuoteCache)
    return characterQuoteCache[c];
  var charCode = c.charCodeAt(0);
  var result;
  if (charCode < 16) result = '\\u000';
  else if (charCode < 256) result = '\\u00';
  else if (charCode < 4096) result = '\\u0';
  else result = '\\u';
  result += charCode.toString(16);
  characterQuoteCache[c] = result;
  return result;
}

function QuoteJSONString(str) {
  var quotable = /[\\\"\x00-\x1f\x80-\uffff]/g;
  return '"' + str.replace(quotable, QuoteSingleJSONCharacter) + '"';
}

function StackContains(stack, val) {
  var length = stack.length;
  for (var i = 0; i < length; i++) {
    if (stack[i] === val)
      return true;
  }
  return false;
}

function SerializeArray(value, replacer, stack, indent, gap) {
  if (StackContains(stack, value))
    throw MakeTypeError('circular_structure', []);
  stack.push(value);
  var stepback = indent;
  indent += gap;
  var partial = [];
  var len = value.length;
  for (var i = 0; i < len; i++) {
    var strP = JSONSerialize($String(i), value, replacer, stack,
        indent, gap);
    if (IS_UNDEFINED(strP))
      strP = "null";
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
  if (StackContains(stack, value))
    throw MakeTypeError('circular_structure', []);
  stack.push(value);
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
          var member = QuoteJSONString(p) + ":";
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
          var member = QuoteJSONString(p) + ":";
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
  if (IS_OBJECT(value) && value) {
    var toJSON = value.toJSON;
    if (IS_FUNCTION(toJSON))
      value = toJSON.call(value, key);
  }
  if (IS_FUNCTION(replacer))
    value = replacer.call(holder, key, value);
  // Unwrap value if necessary
  if (IS_OBJECT(value)) {
    if (IS_NUMBER_WRAPPER(value)) {
      value = $Number(value);
    } else if (IS_STRING_WRAPPER(value)) {
      value = $String(value);
    }
  }
  switch (typeof value) {
    case "string":
      return QuoteJSONString(value);
    case "object":
      if (!value) {
        return "null";
      } else if (IS_ARRAY(value)) {
        return SerializeArray(value, replacer, stack, indent, gap);
      } else {
        return SerializeObject(value, replacer, stack, indent, gap);
      }
    case "number":
      return $isFinite(value) ? $String(value) : "null";
    case "boolean":
      return value ? "true" : "false";
  }
}

function JSONStringify(value, replacer, space) {
  var stack = [];
  var indent = "";
  if (IS_OBJECT(space)) {
    // Unwrap 'space' if it is wrapped
    if (IS_NUMBER_WRAPPER(space)) {
      space = $Number(space);
    } else if (IS_STRING_WRAPPER(space)) {
      space = $String(space);
    }
  }
  var gap;
  if (IS_NUMBER(space)) {
    space = $Math.min(space, 100);
    gap = "";
    for (var i = 0; i < space; i++)
      gap += " ";
  } else if (IS_STRING(space)) {
    gap = space;
  } else {
    gap = "";
  }
  return JSONSerialize('', {'': value}, replacer, stack, indent, gap);
}

function SetupJSON() {
  InstallFunctions($JSON, DONT_ENUM, $Array(
    "parse", JSONParse,
    "stringify", JSONStringify
  ));
}

SetupJSON();
