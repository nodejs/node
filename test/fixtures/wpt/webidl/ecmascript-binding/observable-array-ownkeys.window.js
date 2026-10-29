"use strict";

test(() => {
  const observableArray = document.adoptedStyleSheets;
  assert_array_equals(
      Object.getOwnPropertyNames(observableArray),
      ["length"],
      "Initially only \"length\".");

  observableArray["zzz"] = true;
  observableArray["aaa"] = true;
  assert_array_equals(
      Object.getOwnPropertyNames(observableArray),
      ["length", "zzz", "aaa"],
      "Own properties whose key is a string have been added.");

  observableArray[0] = new CSSStyleSheet();
  observableArray[1] = new CSSStyleSheet();
  assert_array_equals(
      Object.getOwnPropertyNames(observableArray),
      ["0", "1", "length", "zzz", "aaa"],
      "Own properties whose key is an array index have been added.");

  observableArray[Symbol.toStringTag] = "string_tag";
  observableArray[Symbol.toPrimitive] = "primitive";
  assert_array_equals(
      Object.getOwnPropertyNames(observableArray),
      ["0", "1", "length", "zzz", "aaa"],
      "Own properties whose key is a symbol have been added (non-symbol).");
  assert_array_equals(
      Object.getOwnPropertySymbols(observableArray),
      [Symbol.toStringTag, Symbol.toPrimitive],
      "Own properties whose key is a symbol have been added (symbol).");
}, "ObservableArray's ownKeys trap");
