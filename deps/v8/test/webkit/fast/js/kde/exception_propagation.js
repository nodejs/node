// Copyright 2013 the V8 project authors. All rights reserved.
// Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1.  Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
// 2.  Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
// ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

description("KDE JS Test");
var global = this;
function myfunc() {
}
function throwex() {
  throw new Error("test exception");
}

//---------------------------
var func_ret_with_ex_func = 4;
try {
  func_ret_with_ex_func = throwex()();
}
catch (e) {
}
shouldBe("func_ret_with_ex_func", "4");

// ---------------------------------

var func_ret_from_ex_throw_args = 4;
try {
  func_ret_from_ex_throw_args = Math.abs(throwex());
}
catch (e) {
}
shouldBe("func_ret_from_ex_throw_args", "4");

// ---------------------------------

var set_from_throw_func_args = 4;
try {
  throwex()(set_from_throw_func_args = 1);
}
catch (e) {
}
shouldBe("set_from_throw_func_args","4");

// ---------------------------------

var set_from_func_throw_args = 4;
try {
  myfunc(throwex(), set_from_func_throw_args = 1);
}
catch (e) {
}
shouldBe("set_from_func_throw_args","4");

// ---------------------------------

var set_from_before_func_throw_args = 4;
try {
  myfunc(set_from_before_func_throw_args = 1, throwex());
}
catch (e) {
}
shouldBe("set_from_before_func_throw_args","1");

// ---------------------------------

// ### move to function.js
var function_param_order = "";
function aparam() {
  function_param_order += "a";
}
function bparam() {
  function_param_order += "b";
}
function cparam() {
  function_param_order += "c";
}
myfunc(aparam(),bparam(),cparam());
shouldBe("function_param_order","'abc'");

// ---------------------------------
// ### move to function.js
var new_param_order = "";
function anewparam() {
  new_param_order += "a";
}
function bnewparam() {
  new_param_order += "b";
}
function cnewparam() {
  new_param_order += "c";
}
new myfunc(anewparam(),bnewparam(),cnewparam());
shouldBe("new_param_order","'abc'");

// ---------------------------------
// ### move to function.js
var elision_param_order = "";
function aelision() {
  elision_param_order += "a";
}
function belision() {
  elision_param_order += "b";
}
function celision() {
  elision_param_order += "c";
}
[aelision(),belision(),celision()];
shouldBe("elision_param_order","'abc'");

// ---------------------------------
// ### move to function.js
var comma_order = "";
function acomma() {
  comma_order += "a";
}
function bcomma() {
  comma_order += "b";
}
function ccomma() {
  comma_order += "c";
}
acomma(),bcomma(),ccomma();
shouldBe("comma_order","'abc'");

// ---------------------------------

function checkOperator(op,name) {
  var code =(
    "global."+name+"_part1 = 4;\n"+
    "try {\n"+
    "  ("+name+"_part1 = 1) "+op+" throwex();\n"+
    "}\n"+
    "catch (e) {\n"+
    "}\n"+
    "shouldBe('"+name+"_part1', '1');\n"+
    "global."+name+"_part2 = 4;\n"+
    "try {\n"+
    "  throwex() "+op+" ("+name+"_part2 = 1);\n"+
    "}\n"+
    "catch (e) {\n"+
    "}\n"+
    "shouldBe('"+name+"_part2', '4');\n");
//  print("\n\n\n");
//  print(code);
  eval(code);
}

checkOperator("==","OpEqEq");
checkOperator("!=","OpNotEq");
checkOperator("===","OpStrEq");
checkOperator("!==","OpStrNEq");
// ### these generate a syntax error in mozilla - kjs should do the same (?)
//checkOperator("+=","OpPlusEq");
//checkOperator("-=","OpMinusEq");
//checkOperator("*=","OpMultEq");
//checkOperator("/=","OpDivEq");
//                  OpPlusPlus,
//                  OpMinusMinus,
checkOperator("<","OpLess");
checkOperator("<=","OpLessEq");
checkOperator(">","OpGreater");
checkOperator(">=","OpGreaterEq");
//checkOperator("&=","OpAndEq");
//checkOperator("^=","OpXOrEq");
//checkOperator("|=","OpOrEq");
//checkOperator("%=","OpModEq");
checkOperator("&&","OpAnd");
checkOperator("||","OpOr");
checkOperator("&","OpBitAnd");
checkOperator("^","OpBitXOr");
checkOperator("|","OpBitOr");
checkOperator("<<","OpLShift");
checkOperator(">>","OpRShift");
checkOperator(">>>","OpURShift");
//                  OpIn,
checkOperator("instanceof","OpInstanceOf");

// ---------------------------------
var set_from_if_stmt = 4;
try {
  if (throwex()) {
    set_from_if_stmt = 1;
  }
}
catch (e) {
}
shouldBe("set_from_if_stmt","4");

// ---------------------------------
var set_from_if_else_stmt = 4;
try {
  if (throwex()) {
    set_from_if_else_stmt = 1;
  }
  else {
    undefined;
  }
}
catch (e) {
}
shouldBe("set_from_if_else_stmt","4");

// ---------------------------------

var set_from_else_in_if_else_stmt = 4;
try {
  if (throwex()) {
    undefined;
  }
  else {
    set_from_else_in_if_else_stmt = 1;
  }
}
catch (e) {
}
shouldBe("set_from_else_in_if_else_stmt","4");

// ---------------------------------

var comma_left = 4;
try {
  comma_left = 1, throwex();
}
catch (e) {
}
shouldBe("comma_left","1");

// ---------------------------------

var comma_left = 4;
try {
   throwex(), comma_left = 1;
}
catch (e) {
}
shouldBe("comma_left","4");

var vardecl_assign_throws = 4;
try {
  var vardecl_assign_throws = throwex();
}
catch (e) {
}
shouldBe("vardecl_assign_throws","4");

// ---------------------------------

var var_assign_before_throw_run = false;
function var_assign_before_throw() {
  var_assign_before_throw_run = true;
  return 1;
}

var var_assign_after_throw_run = false;
function var_assign_after_throw() {
  var_assign_after_throw_run = true;
  return 1;
}

try {
  var var_assign1 = var_assign_before_throw(),
      var_assign2 = throwex(),
      var_assign1 = var_assign_before_throw();
}
catch (e) {
}
shouldBe("var_assign_before_throw_run","true");
shouldBe("var_assign_after_throw_run","false");

// ---------------------------------

var do_val = 4;
try {
  do {
    do_val++;
  }
  while (throwex());
}
catch (e) {
}
shouldBe("do_val","5");

// ---------------------------------
var while_val = 4;
try {
  while (throwex()) {
    while_val++;
  }
}
catch (e) {
}
shouldBe("while_val","4");

// ---------------------------------
var for_val_part1_throw2 = 4;
try {
  for (for_val_part1_throw2 = 1; throwex(); ) {
  }
}
catch (e) {
}
shouldBe("for_val_part1_throw2","1");

// ---------------------------------
var for_val_part1_throw3 = 4;
try {
  for (for_val_part1_throw3 = 1; ; throwex()) {
  }
}
catch (e) {
}
shouldBe("for_val_part1_throw3","1");

// ---------------------------------
var for_val_part2_throw1 = 4;
try {
  for (throwex(); for_val_part2_throw1 = 1; ) {
  }
}
catch (e) {
}
shouldBe("for_val_part2_throw1","4");

// ---------------------------------
var for_val_part2_throw3 = 4;
try {
  for (; for_val_part2_throw3 = 1; throwex()) {
  }
}
catch (e) {
}
shouldBe("for_val_part2_throw3","1");

// ---------------------------------
var for_val_part3_throw1 = 4;
try {
  for (throwex(); ; for_val_part3_throw1 = 1) {
  }
}
catch (e) {
}
shouldBe("for_val_part3_throw1","4");

// ---------------------------------
var for_val_part3_throw2 = 4;
try {
  for (; throwex(); for_val_part3_throw2 = 1) {
  }
}
catch (e) {
}
shouldBe("for_val_part3_throw2","4");

// ---------------------------------
var for_val_part1_throwbody = 4;
try {
  for (for_val_part1_throwbody = 1; ;) {
    throwex();
  }
}
catch (e) {
}
shouldBe("for_val_part1_throwbody","1");

// ---------------------------------
var for_val_part2_throwbody = 4;
try {
  for (; for_val_part2_throwbody = 1; ) {
    throwex();
  }
}
catch (e) {
}
shouldBe("for_val_part2_throwbody","1");

// ---------------------------------
var for_val_part3_throwbody = 4;
try {
  for (; ; for_val_part3_throwbody = 1) {
    throwex();
  }
}
catch (e) {
}
shouldBe("for_val_part3_throwbody","4");

// ---------------------------------
var set_inside_with_throw = 4;
try {
  with (throwex()) {
    set_inside_with_throw = 1;
  }
}
catch (e) {
}
shouldBe("set_inside_with_throw","4");

// ---------------------------------
var set_inside_with_cantconverttoobject = 4;
try {
  with (undefined) {
    print("FAIL. This message should not be displayed");
    set_inside_with_cantconverttoobject = 1;
  }
}
catch (e) {
}
shouldBe("set_inside_with_cantconverttoobject","4");
// ### test case, sw
