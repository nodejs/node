// Copyright 2008 the V8 project authors. All rights reserved.
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

// Make sure the 'constructor' property isn't enumerable.
var enums = "";
for (var k in this) enums += (k + '|');
assertEquals(-1, enums.split('|').indexOf("constructor"));

// Make sure this doesn't crash.
new this.constructor;
new this.constructor();
new this.constructor(1,2,3,4,5,6);

var x = 0;
try {
  eval("SetValueOf(typeof(break.prototype.name), Math.max(typeof(break)))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("export Join((void), false.className(), null instanceof continue, return 'a', 0.__defineGetter__(x,function(){native}))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("with ({ void&&null.push(goto NaN) : Math.max(undef).toText }) { {-1/null,1.isNull} }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new break>>>=native.charCodeAt(-1.valueOf())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Number(this > native)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new {native,0.2}?continue+undef:IsSmi(0.2)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = break.toString()&&return continue")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (-1==continue.toJSONProtocol, GetFunctionFor(break.call(NaN)), (!new RegExp).prototype.new Object()<<void) { debugger.__defineSetter__(null,function(){continue})>>>=GetFunctionFor(-1) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (parseFloat(NaN).splice() in null.add(1).className()) { true[0.2]<<x.splice() }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("let (debugger.constructor.valueOf()) { this.sort().true.splice() }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("unescape(break.toObject()).prototype.new RegExp.continue.__lookupGetter__(x.slice(1, NaN)) = typeof(null.push(0.2))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Date(Iterator(continue.pop()))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("function X(x) { return new RegExp.shift().concat({debugger,continue}) }; X(return goto 0)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("Instantiate(0.add(break)&&x > null)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("with ({ eval(Array(x)) : 1.call('a').superConstructor }) { debugger.lastIndex.toLocaleString() }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = return true.__defineGetter__(this,function(){0.2})")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = new typeof(0)&this.lastIndex")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("String(new RegExp.call(1)).prototype.unescape(parseFloat(-1)) = false<<true.x.lastIndexOf(1)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("with ({ 1+debugger.valueOf() : continue.join().name() }) { parseInt(true)==undef.sort() }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new RegExp>>0.2.superConstructor.prototype.eval(void).className() = false.join().prototype.name")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("export (new Object()?undef:native)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = new null.isNull.slice(x.prototype.value, Iterator(undef))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("export function () { 0.2 }.unshift()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Math.max(continue.valueOf())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = return debugger.toObject()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("switch (-1.length+new Object().prototype.name) { case (debugger.constructor.sort()): IsPrimitive(undef.__defineSetter__(undef,function(){native})); break; }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("delete (!new Object().toLocaleString())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Date(0<<'a'>>>=new RegExp['a'])")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("native {unescape(true),new RegExp.isNull}")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = -1.lastIndexOf(false)?parseFloat(void):Join(null, continue, new Object(), x, break)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("label null/void-break.__lookupGetter__(native)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Function(0.2.join().constructor)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("label function () { false }.__lookupGetter__(this==1)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("Instantiate(-1.prototype.0.2.unshift())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = new return goto -1")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new {Number(debugger)}")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("if (parseInt(break) instanceof 0.length) { this.(!0.2) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("SetValueOf(break.superConstructor[throw new false(true)], this.~x)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("SetValueOf(function () { IsSmi(-1) }, unescape(IsPrimitive(void)))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (new RegExp.join().className() in new Object().length()>>true.toObject()) { parseFloat(escape(debugger)) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = new String(debugger).toJSONProtocol")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("SetValueOf(1.indexOf('a')<<break.__lookupGetter__('a'), new Object().null.prototype.new RegExp.charCodeAt(-1))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = new {parseInt(0)}")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Date(void.join().add(escape(undef)))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("native parseFloat(false.charAt(new RegExp))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Date(~Iterator(void))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Function(NaN.shift().toJSONProtocol)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Date(native-debugger<<continue.slice(x, new RegExp))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = parseFloat(~new Object())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (null.size/true.add(void) in 0+continue&true.null) { continue.toObject()/throw new true(debugger) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (Iterator(native+break) in debugger.superConstructor.constructor) { Math.max(0.add(undef)) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new {-1.add(native),true.sort()}")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new {IsSmi(break),throw new 'a'(null)}")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("switch (parseInt(0).length()) { case ('a'.toObject().__defineSetter__(GetFunctionFor(null),function(){(!x)})): IsSmi(void).constructor; break; }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = new 0.lastIndexOf(NaN).shift()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("with ({ 0>>>=this.lastIndex : new Object().lastIndexOf(true).toObject() }) { x.lastIndex > 1.__defineSetter__(false,function(){this}) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("with ({ throw new false(0.2).prototype.name : parseFloat(false)+(!debugger) }) { escape(undef.lastIndex) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("Math.pow(0.2).toJSONProtocol.prototype.break.superConstructor.slice(NaN.exec(undef), -1.lastIndexOf(NaN)) = true.splice().length")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("native continue.className().constructor")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("let (0.2.isNull&undef.toString()) { continue/void+parseInt(null) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = new Math.pow(break==this)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("SetValueOf(continue.__lookupGetter__(null).constructor, debugger.filter(0.2)>>>=this.'a')")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("with ({ 0.2.unshift() > true.size : return Math.max(new RegExp) }) { void.splice().toString() }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new unescape(false).unshift()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("function X(x) { return this.true?'a'==this:0.2.__lookupGetter__(void) }; X(Iterator(false).length)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = function () { null }.__defineSetter__(0.charCodeAt(new Object()),function(){null>>>=new Object()})")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("import goto 'a'.charAt(native.className())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("import 0.2.isNull.__lookupGetter__(debugger.size)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (~new Object().push(Array(null)) in new RegExp>>>=void.prototype.name) { goto break.lastIndex }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("delete String(x).slice(String('a'), parseFloat(false))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new parseInt(continue.__defineGetter__(0.2,function(){1}))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("Instantiate(true.concat(undef)==0.2.new RegExp)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("function X(x) { return NaN['a']?-1.exec(0):NaN.prototype.this }; X(native.prototype.name.toLocaleString())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (debugger==continue.toObject(), Array(NaN.className()), Math.max(new RegExp).prototype.value) { GetFunctionFor('a').prototype.value }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = new parseInt(break)==Array(x)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (parseInt(0.2.charCodeAt(this)), this.continue.prototype.name, native.superConstructor.superConstructor) { Join(0.__defineGetter__(continue,function(){undef}), {1}, parseFloat(0), undef.__defineSetter__(break,function(){null}), x?-1:-1) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("export Join(debugger.splice(), parseInt(NaN), new RegExp.pop(), this.false, x.-1)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = Math.max(native).charCodeAt(continue==break)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (void==NaN.sort(), new Object()==new RegExp.toObject(), -1/NaN.unshift()) { GetFunctionFor(true).name() }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for ((!'a'.join()), ~NaN.__defineGetter__(undef,function(){this}), Math.pow(NaN).__lookupGetter__(typeof(false))) { throw new debugger.toObject()(Math.max(-1)) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (NaN.shift()&&undef&&continue in throw new x(NaN).prototype.-1&x) { return native.toJSONProtocol }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = new (0).charAt(this.charCodeAt(new Object()))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("function X(x) { return x.valueOf().size }; X(0.2.unshift().unshift())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("if (eval(new Object().valueOf())) { break.prototype.name.__defineGetter__(eval(NaN),function(){Math.max(native)}) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (Math.pow(1).isNull in Iterator(continue.length())) { Join(true, 0.2, null, x, new Object()).length }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("SetValueOf(0>>>=void.unshift(), void.exec('a').undef.length())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("delete throw new this(0.2).pop()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Iterator(unescape(continue))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("function X(x) { return unescape(goto debugger) }; X(new RegExp.push(break).name())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = undef/'a'.indexOf(-1.exec(false))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (continue.isNull.filter(this.toText), function () { throw new 'a'(0.2) }, native?break:undef.prototype.return continue) { Array(void.toText) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = new this.slice(new Object(), 1).isNull")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (0.2.className().call((!debugger)), native.__defineGetter__(0,function(){x}).name(), null.splice().splice()) { NaN.charCodeAt(new Object()) > true.toString() }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("native false.length?new RegExp instanceof this:Array(undef)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new ~0.2.call(typeof(false))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Number(0.2.sort())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new x.join().shift()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("switch (~new Object().toText) { case (new RegExp.unshift().exec(new RegExp<<debugger)): -1.length.exec(this.isNull); break; }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new parseInt(~true)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new unescape(debugger.call(null))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = new GetFunctionFor(0.2).toObject()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("delete IsPrimitive(null.join())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (eval(0.2) instanceof debugger.splice() in null.superConstructor==new Object()&void) { Number(0+x) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("let ('a'-continue?null.length():escape(continue)) { return undef.push(false.shift()) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (Array(x.length) in 'a'.length().sort()) { goto (new Object()) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("let (NaN==true.length) { IsPrimitive(0.2).prototype.value }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("SetValueOf(return true&&void, new RegExp.toObject().length())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Math.pow(void).length")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Function(void.add(continue).charCodeAt(this.toObject()))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("export Join(break.toObject(), 0.2.isNull, false.call(0), break.filter(break), 1.length())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("if (1/NaN.__lookupGetter__(undef.prototype.value)) { escape(eval(this)) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Function(Join(unescape(x), new RegExp.__defineGetter__(debugger,function(){NaN}), 'a'.indexOf(0.2), false.prototype.name, (this)))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = new Math.pow(native).indexOf(1>>>=-1)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new RegExp?native:continue.join().prototype.Math.max(x.__defineSetter__(1,function(){continue})) = parseFloat(parseInt(null))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("native function () { new RegExp }.new RegExp.pop()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("import typeof(new RegExp.valueOf())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("switch (0.2.size>>NaN-continue) { case ('a'.push(true).indexOf(NaN.lastIndexOf(-1))): {0.2,x}.toObject(); break; }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("if (IsSmi(new Object())/false.filter('a')) { function () { Iterator(debugger) } }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = break.lastIndex.size")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Function(new Object() > 0.length())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("native IsPrimitive(continue)==break.charCodeAt(new Object())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new break.true<<'a'-NaN")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Number(-1?'a':-1)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (parseFloat('a'.exec(continue)) in (!new RegExp)&&0.2.toObject()) { {true,x}.add(void.prototype.NaN) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("let (-1.prototype.value.join()) { (!1.prototype.name) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new GetFunctionFor(continue).toJSONProtocol")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (Math.pow(continue.slice(null, native)), goto (!0), native?1:this.charAt(String(debugger))) { parseFloat(~this) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("SetValueOf(debugger.pop().length, new RegExp.isNull.toText)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (typeof(new RegExp.slice(new RegExp, 0)) in native.toLocaleString().lastIndexOf(0.2.length())) { native>>>=new RegExp.length() }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("native x.join().className()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new 0?0:true.toLocaleString()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = IsPrimitive(0).concat(new Object().name())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = new parseFloat(x)?this.valueOf():IsSmi(x)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = new 'a'.slice(null, -1).shift()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("label 'a'+void.concat('a'>>>=-1)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Function(escape(0.length))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = parseInt(0.lastIndexOf(NaN))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("SetValueOf(null&debugger.valueOf(), 0[false].push(false.add(debugger)))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = parseInt(new RegExp.__lookupGetter__(break))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("SetValueOf(~false&&break>>0, new RegExp.lastIndex.add({this}))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = Join(break, continue, 0, debugger, NaN).toLocaleString()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("import new Object().sort().superConstructor")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = new IsSmi(goto -1)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("function X(x) { return Iterator(null).toObject() }; X(-1==new Object()==0.__lookupGetter__(native))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("native void.join().add(parseFloat(continue))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("let (function () { -1 }.shift()) { escape(1.unshift()) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Function(new RegExp.indexOf(1).filter(continue instanceof break))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("if (NaN?continue:NaN.shift()) { native.push(null).add(new Object().superConstructor) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("function X(x) { return new Object().length().toText }; X(debugger.indexOf(this).toText)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = new Object().call('a').charCodeAt(native.size)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new function () { continue }.add(true.slice(continue, new RegExp))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x[native] instanceof -1.join().prototype.this.null.size = 0.2.prototype.x+0.2.indexOf(false)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (this instanceof new RegExp.splice() in null>>>=new RegExp.valueOf()) { function () { unescape(1) } }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (true.shift()/native.null in undef.call(NaN).isNull) { native+this-x.size }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("function X(x) { return false.pop()<<Join(continue, false, break, NaN, -1) }; X(IsSmi(debugger>>x))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("if ({parseFloat(null),Math.max(native)}) { 0.2-new Object().__lookupGetter__(eval(new Object())) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("SetValueOf(Array(1).toLocaleString(), null.name().exec(undef.filter(false)))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Function(true.filter(this).pop())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("let (break.lastIndex.superConstructor) { new Object().toString().length() }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("label (!0.2/debugger)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("with ({ NaN.concat(new RegExp)+Join(1, false, new Object(), new Object(), x) : unescape(x).concat(Iterator(-1)) }) { 'a'.isNull.__lookupGetter__(this+native) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("export break.name()/IsPrimitive(this)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new {null}.prototype.value")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new true+false.__lookupGetter__(null&continue)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("if (-1.push(new RegExp)[void.valueOf()]) { new RegExp.className().__lookupGetter__(Array(0)) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("export NaN.__lookupGetter__(undef).__lookupGetter__(void.isNull)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("with ({ ~new RegExp.filter(undef&&this) : String(continue)<<NaN.toText }) { this.exec(this).length }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (true&void.exec(void.exec(continue)) in Join('a', undef, new Object(), continue, x) instanceof {undef}) { unescape(-1.prototype.name) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("import void.push(true).join()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("SetValueOf({break}&x.name(), 1.charAt(false).slice(continue.superConstructor, this&&break))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("let (this.call(this) > Iterator(continue)) { new Object().prototype.value.slice(1.slice(native, -1), (!false)) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("export parseInt(new RegExp>>>=x)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (escape(x==debugger), NaN.shift()&debugger?false:0.2, (!new RegExp)&goto break) { unescape(x.toText) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Date(throw new NaN.toObject()(this?break:true))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new (typeof(this))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (unescape('a'/0) in ~new Object().lastIndex) { IsSmi(0).push(0.concat(0.2)) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("(!new RegExp)[0.2 > new Object()].prototype.Number(debugger.join()) = native&-1.size")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new false.toJSONProtocol&&0.2.constructor")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (~0?0.2:undef in new RegExp.charCodeAt(0).prototype.name) { NaN.toLocaleString().splice() }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (~IsPrimitive(new RegExp), true.toString().size, null.charCodeAt('a') > null.concat(0)) { break.toJSONProtocol/IsPrimitive(break) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new parseInt(new Object()).lastIndexOf(NaN > void)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("export break.splice()&&-1.prototype.new Object()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("{{true,0}}.prototype.break.length.splice() = 'a'.toText.superConstructor")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("let (debugger>>>=continue > break.exec(1)) { Math.pow(new RegExp)==NaN>>>=0.2 }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("with ({ 0.2==0.2/goto true : IsSmi(native).isNull }) { throw new {x,null}(false.className()) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = {false.concat(null),Math.pow(NaN)}")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("export Array(null).add(NaN.valueOf())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (parseFloat(new Object()==true) in GetFunctionFor('a'&false)) { native&undef.toJSONProtocol }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = new {eval(null),(debugger)}")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("import {this.0,debugger.filter(NaN)}")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("import break.charAt(-1)<<false.__defineSetter__(0,function(){x})")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = goto false > new Object()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("null.superConstructor[debugger.isNull].prototype.Math.max('a').shift() = parseInt(0).size")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("native eval(void.add(break))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Date(x > void.join())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("with ({ {this.toObject()} : Number(NaN).toJSONProtocol }) { 0.2.className().prototype.name }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("if (false.__defineGetter__(undef,function(){undef}).exec(NaN.splice())) { typeof(Join(void, new RegExp, break, -1, -1)) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (false.splice().toObject(), continue.name().size, Join(void?debugger:this, new RegExp.__defineSetter__(NaN,function(){NaN}), x.unshift(), this.true, parseInt(break))) { undef<<continue.toText }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("let (this.0.indexOf(break)) { break.charAt(this).unshift() }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("import Join(new Object().splice(), this instanceof 1, parseFloat(NaN), undef.concat(x), void.className())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Function(goto NaN.toString())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("label 'a'<<break.shift()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = Iterator(continue)[new Object()>>NaN]")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = Join(new RegExp, 'a', this, void, true)>>>=continue>>native")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("import new Object().toJSONProtocol.splice()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("function X(x) { return undef.__defineSetter__(native,function(){void}).toJSONProtocol }; X(eval(x).charCodeAt('a'.concat(true)))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("Instantiate(throw new 0.2.__defineGetter__(NaN,function(){-1})(void&&new RegExp))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = 0.unshift() > IsSmi(NaN)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("label x.call(null).lastIndex")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("SetValueOf(IsSmi(0.2.add(0)), x.add(break).this.__defineGetter__(undef,function(){new RegExp}))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("native Number(this).toObject()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new NaN.shift().add(String(new Object()))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new null.name().splice()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = 1.undef.push(new Object().call(null))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Function(parseInt(1).size)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = this.x.sort()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("Instantiate(continue.valueOf().prototype.new RegExp.splice())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("Instantiate(this.charAt(continue)?undef+'a':unescape(1))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("SetValueOf({throw new 'a'(0.2),void.lastIndexOf(NaN)}, Math.pow(new Object().className()))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("if (1.slice(new Object(), this).valueOf()) { parseInt(true).pop() }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("with ({ 0.2.superConstructor.lastIndex : goto debugger<<Join(undef, 1, true, undef, debugger) }) { function () { NaN }.prototype.name }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("-1.exec(debugger).length.prototype.debugger > null.slice(Iterator(void), continue.concat(0)) = parseInt(throw new 1(1))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Date(new Object().constructor.call(Number(1)))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = new null.unshift().call(escape(x))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("switch (Math.pow(native).toLocaleString()) { case (false instanceof native.join()): Math.pow(NaN).size; break; }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("label function () { new Object() }.prototype.true.size")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = Join('a', 0.2, false, new Object(), void).continue.className()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = IsPrimitive(break.__lookupGetter__(-1))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = new Object()>>0.2.prototype.name")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = new IsPrimitive(new Object()).shift()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("if (Array(parseInt(break))) { 'a'.toString().unshift() }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = return 0.2>>>=-1?undef:undef")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Object().splice().unshift().prototype.null&&native.__lookupGetter__(undef>>>=NaN) = (1<<break)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("delete NaN.charAt(1).concat(NaN.0.2)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("Instantiate(new RegExp.sort().toJSONProtocol)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("function X(x) { return GetFunctionFor(false).lastIndexOf(1.shift()) }; X(this.0.2.charCodeAt(0.2))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (goto NaN.toObject(), ~true.'a', parseInt(debugger)+eval(false)) { eval(0.2.constructor) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("switch (parseInt(debugger).pop()) { case (this.push(true).valueOf()): Join(continue, debugger, native, native, debugger).filter(Array(continue)); break; }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = new debugger.sort() instanceof this>>1")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("with ({ parseFloat(false).prototype.(!new Object()) : {unescape(-1)} }) { Math.max(new RegExp.superConstructor) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("Instantiate({Math.pow(break)})")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("import typeof(break.valueOf())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("Instantiate(Math.pow(-1[new RegExp]))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("native IsPrimitive(1).concat({x,null})")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("NaN.length.prototype.value.prototype.function () { null==new Object() } = break.name()&IsPrimitive(0)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("delete NaN.prototype.-1.toString()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new continue.unshift()+parseFloat(undef)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = new NaN-break.call(false.pop())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("native new RegExp.exec(break).pop()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("SetValueOf({'a',null}.prototype.value, 1.shift() instanceof {'a',0})")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (debugger.valueOf().size, function () { x.unshift() }, IsSmi(1)&&true==native) { new Object().__defineGetter__(this,function(){'a'})&&eval(native) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("export 'a'.pop().charCodeAt(x.className())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("export String(IsSmi(debugger))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("typeof(debugger).valueOf().prototype.(1).lastIndexOf(this.break) = x.prototype.name.toLocaleString()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("native Array(typeof(false))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Function(1.__defineGetter__(1,function(){1}).null.constructor)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = 1.charAt(0).toObject()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Date(Math.max('a'.filter(new Object())))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Date(void.prototype.name.unshift())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (-1.toJSONProtocol.call(-1.size) in ~x.sort()) { eval(0&debugger) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for ('a'==undef.join() in Math.pow(IsSmi(false))) { undef > this>>goto x }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("Instantiate('a'.constructor.isNull)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (GetFunctionFor(this.slice(0.2, this)), this.prototype.void?null.unshift():native.className(), Number(new Object().call(-1))) { 0.splice() > debugger&&this }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("with ({ {goto new RegExp,Join(new Object(), native, continue, -1, x)} : NaN&x/{0,break} }) { this.lastIndexOf(new RegExp).join() }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("let (typeof(break.length())) { native&&false.sort() }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = new parseFloat(-1 instanceof break)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("label throw new continue.unshift()(null.shift())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("import Math.max(0.2.toLocaleString())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("function X(x) { return false.unshift().className() }; X(escape(NaN&NaN))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Date(Join(native.toText, goto x, 0.2.splice(), Join('a', 0, void, NaN, 1), eval(native)))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("if (GetFunctionFor(true.prototype.name)) { parseInt(NaN).toLocaleString() }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new escape(native).__defineSetter__(return native,function(){undef > native})")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = new typeof(true > 'a')")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("switch (debugger.prototype.0.2<<new RegExp+false) { case (native.splice().filter({x})): false&true.indexOf(1.__defineGetter__(native,function(){continue})); break; }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("label true-NaN.prototype.native.shift()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new typeof(new RegExp.splice())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("switch (function () { this.NaN }) { case (this.continue.prototype.parseFloat(false)): IsPrimitive(new Object()-'a'); break; }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("export break.__lookupGetter__(debugger).indexOf(native.pop())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("switch (GetFunctionFor(NaN.lastIndex)) { case (new RegExp.lastIndex.toLocaleString()): NaN.join().indexOf(eval(-1)); break; }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("native {void.charAt(true)}")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new new Object()==NaN.join()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Date(typeof(Array(new Object())))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("label throw new (false)(eval(x))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new new RegExp.size.charAt(true > -1)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = debugger.toObject().charAt(this<<undef)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("with ({ 'a'.valueOf()+parseInt(undef) : IsPrimitive(null).lastIndex }) { NaN.toObject().isNull }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = new new Object()&&void.lastIndexOf(0.2.splice())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("with ({ 1+1.name() : Join(Math.pow(debugger), new RegExp-1, x > 1, x<<-1, new RegExp.size) }) { undef[undef].size }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("delete native.call(-1).isNull")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("if (new Object()>>>=break==Math.pow(debugger)) { IsPrimitive(this).lastIndex }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for ((!x&&new RegExp) in undef.toLocaleString().slice(new RegExp.indexOf(NaN), IsPrimitive(-1))) { false.size+debugger[x] }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("import 0.length.__defineGetter__(0.2.shift(),function(){'a'.className()})")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("Instantiate(goto new Object().push(void))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("with ({ Array(this.0) : parseFloat(void).pop() }) { escape(true).slice(continue.lastIndex, false.toObject()) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new native==true.filter({NaN,-1})")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for ('a'.__defineSetter__(continue,function(){-1}).unshift(), Array(undef).toLocaleString(), undef.__lookupGetter__(void).toLocaleString()) { parseInt(false/native) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("this.x<<false.prototype.true.toLocaleString()==NaN.pop() = this.superConstructor>>Math.max(true)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("function X(x) { return this.prototype.name.splice() }; X(unescape(x).__lookupGetter__(Number(debugger)))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = new (!NaN).unshift()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Function(escape(Iterator(this)))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("function X(x) { return Number(new RegExp)<<this?true:-1 }; X(Number(null).lastIndex)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("export this.void.splice()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (this.prototype.null.sort() in -1.className()&void.filter(new Object())) { GetFunctionFor(new Object()).pop() }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("label 0[break].sort()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (null.length().toString(), eval(-1).toObject(), (!continue.concat(continue))) { true.name()/native<<new RegExp }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (unescape(null).sort(), Number(undef).charCodeAt(IsPrimitive(NaN)), null>>true/null.join()) { 0.2.toObject() > IsPrimitive(new RegExp) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Date({NaN,native}&&1+undef)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("Instantiate(IsPrimitive(undef>>>=1))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (Join(true, 'a', true, 1, NaN).add({1}), GetFunctionFor(new Object().push(new Object())), goto 1.length) { Math.pow(GetFunctionFor(native)) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("function X(x) { return break.isNull > parseInt(continue) }; X((new RegExp instanceof 1))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("with ({ Number(false).indexOf(x instanceof new Object()) : function () { x.toString() } }) { false.name().indexOf(GetFunctionFor(null)) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Date('a'.constructor.prototype.name)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("GetFunctionFor(void&new Object()).prototype.debugger.add(null)[void.unshift()] = new RegExp.isNull.Iterator(this)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("delete false?break:undef.constructor")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("with ({ (native.filter(1)) : eval(this&&0.2) }) { undef.length instanceof new Object().toText }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("export String(break.lastIndexOf(null))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("label (!Iterator(new RegExp))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("SetValueOf(String(null==-1), {1&0})")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Date(parseInt('a' > 0))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("SetValueOf(debugger.toJSONProtocol.indexOf(escape(0)), this.filter(null).__defineSetter__(continue.break,function(){debugger>>null}))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("this.name().length().prototype.goto false.exec(true.charCodeAt(continue)) = Join(-1-false, undef.superConstructor, 'a'.shift(), (!x), NaN.this)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Function(typeof(new RegExp).sort())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new 0.2.concat(x).splice()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (goto void.indexOf(throw new x(1)), typeof(return new RegExp), IsPrimitive(-1).add(void.lastIndexOf(debugger))) { null.indexOf(void).toText }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("return new RegExp.pop().prototype.String(x.toObject()) = 1.superConstructor.charCodeAt(new RegExp.charCodeAt(null))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new null&true.prototype.name")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = -1>>>=NaN.indexOf((debugger))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = new parseFloat(null).splice()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("import -1.lastIndexOf(new RegExp) instanceof throw new void(0.2)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("if ((0.shift())) { Join(IsPrimitive(-1), break.__defineSetter__(true,function(){break}), parseInt(null), parseFloat(break), true/null) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = new escape(1 > continue)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("switch (parseInt(undef)>>false.filter(continue)) { case (this.undef/new Object()): 'a'.toJSONProtocol.__defineGetter__(new RegExp-undef,function(){parseFloat(new RegExp)}); break; }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("{void}.shift().prototype.this.Array(new Object()) = {0.2,new RegExp}.lastIndexOf(break.splice())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new continue&&new Object().lastIndexOf(new Object() instanceof 1)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (throw new 'a'.exec(x)(return false), native/void.constructor, {native}==true.toLocaleString()) { goto 1 instanceof 1.isNull }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (break.concat(break) > native>>>=-1, (debugger.x), Join(x, void, void, new RegExp, null).name()) { void.charCodeAt(true).valueOf() }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = new 'a'>>0 instanceof new Object().push(new RegExp)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("if (return ~break) { break.__defineGetter__(break,function(){-1}).shift() }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("Instantiate(Join(null, -1, undef, null, 0).toString())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("let ({new RegExp,void}.slice(break.isNull, false.shift())) { eval(debugger.slice(this, 1)) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("function X(x) { return {GetFunctionFor(0)} }; X('a'.prototype.debugger.concat(void.constructor))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("let (~true instanceof continue) { escape(new RegExp.toObject()) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("escape(0[native]).prototype.debugger.add(1).unshift() = (true.join())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (unescape(void).length, undef.toObject() instanceof x.toObject(), 0.2+true.concat(true.__lookupGetter__(this))) { (x).toJSONProtocol }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("Instantiate(escape(null).__lookupGetter__(undef.size))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("label Array(continue[false])")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("function X(x) { return Number(this&&false) }; X(NaN.toJSONProtocol.toJSONProtocol)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("null.toString().shift().prototype.Array(x).__lookupGetter__('a'.prototype.x) = {1.length,break.join()}")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = new 1.charCodeAt(break)+IsSmi(false)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("SetValueOf(String(this) > 0.2.toText, new RegExp.length.lastIndexOf(1<<0.2))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("switch (new RegExp.pop().charAt(IsSmi(new RegExp))) { case (native.indexOf(this)/native.lastIndex): this.debugger.indexOf(debugger); break; }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Date(Number(x)[debugger.prototype.break])")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("function X(x) { return new RegExp>>>=x.unshift() }; X(Math.max(continue.name()))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("Instantiate(IsSmi(null.size))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = native?0.2:1+GetFunctionFor(void)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (IsPrimitive(-1)>>>=break.valueOf() in String(0 > 0.2)) { Math.max(true.length()) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("switch (escape(unescape(NaN))) { case (Math.pow(eval(undef))): true.charAt(null)&new RegExp.pop(); break; }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("delete Join(new RegExp, 1, false, new Object(), this).toLocaleString()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("label return x.filter(x.join())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new new RegExp.pop().shift()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = new (!debugger.size)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("label Math.max(debugger.__lookupGetter__(NaN))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("Instantiate(eval(debugger[debugger]))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new 0.2.filter(true)&throw new true(debugger)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Date(continue.exec(debugger) > Math.pow(0.2))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("void.prototype.value.name().prototype.Number(undef&NaN) = false.__lookupGetter__(-1).name()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("Instantiate(null.__defineGetter__(native,function(){continue}).valueOf())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("with ({ {new Object()[continue],native.length()} : undef.name().superConstructor }) { Math.pow(break).indexOf(0.toJSONProtocol) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("switch (Iterator(native.call(new RegExp))) { case (String(new RegExp).isNull): goto new RegExp.pop(); break; }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = new x.constructor instanceof undef.indexOf(-1)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("SetValueOf(this.~null, continue.pop()&0&'a')")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("switch (GetFunctionFor(~0)) { case ('a'.'a'<<undef.__defineGetter__(false,function(){true})): (!1).lastIndex; break; }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("function X(x) { return debugger.unshift().0.toString() }; X(Number(break).0.2>>>=false)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Date(Iterator(x)/undef.pop())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("SetValueOf(undef.join().toLocaleString(), null.add(false).valueOf())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("IsSmi(x).toString().prototype.0>>continue.indexOf(NaN.__lookupGetter__(new Object())) = ~-1&typeof(0)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (continue.__lookupGetter__(new RegExp).toObject(), false-0.toString(), return native.sort()) { new RegExp.name().className() }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("switch (escape(new RegExp).toString()) { case (goto eval(1)): this.filter(new Object()).call(new RegExp.slice(null, this)); break; }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = debugger-false.toText")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = Number(null>>new RegExp)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("delete this&native.indexOf('a'.splice())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("SetValueOf(~Math.max(break), 0.2.valueOf().length)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("Instantiate(Number(native.charCodeAt(x)))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = new goto continue.add(0)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("delete typeof(debugger).name()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("'a'<<false.toText.prototype.throw new true(1).lastIndex = 'a'.name().length")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("native 'a'.indexOf(debugger).charAt(NaN.add(new Object()))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("SetValueOf(break>>false.toString(), (false.indexOf(this)))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("delete goto NaN==(!debugger)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Date(0.2.join().superConstructor)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = new this.void.toLocaleString()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("SetValueOf(x.exec(debugger)[GetFunctionFor(0)], native.toObject().exec(new RegExp.sort()))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("Instantiate(0.2.valueOf().toLocaleString())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Function(-1.toJSONProtocol.prototype.name)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Date(Array(-1.shift()))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("export break.concat(undef).unshift()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("native parseFloat(-1)?NaN.toText:debugger.toString()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (void-continue/continue.prototype.undef in String(break.toText)) { parseInt(false).isNull }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("Instantiate(true.isNull.toObject())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("with ({ typeof(debugger).toObject() : x.constructor>>>=null.__defineGetter__(native,function(){debugger}) }) { unescape(undef.lastIndexOf(false)) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("export unescape(continue)<<native[0]")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("if (String(0).unescape(debugger)) { {break.pop(),0.2.constructor} }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("String({true}).prototype.break.length.call(false > 0.2) = GetFunctionFor(0.prototype.new RegExp)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("with ({ false.push(0.2).indexOf(Math.max(debugger)) : x&x.prototype.name }) { goto 1.lastIndex }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Function(0.2.lastIndex&0.2?break:NaN)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = -1.prototype.value.toText")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("import native.toLocaleString()-1.prototype.0")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("export debugger[-1].indexOf(Join(new Object(), 0, x, new Object(), 0.2))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("function X(x) { return (!true).lastIndexOf(true.splice()) }; X(NaN.toString().prototype.value)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("function X(x) { return continue.slice(-1, 1).prototype.true.name() }; X('a'.push(void).prototype.value)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (goto new RegExp.length(), x.sort().className(), Math.max(new RegExp.toJSONProtocol)) { (IsSmi(-1)) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = 0.splice()&&-1.sort()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("let (Math.max(-1>>1)) { break.toLocaleString().toJSONProtocol }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new {void.prototype.break,new RegExp.toString()}")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new IsSmi(debugger).name()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new 'a'.concat(undef).sort()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = new {debugger.toObject(),'a' > false}")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("if (goto 1.concat(Join(x, undef, native, x, new Object()))) { new RegExp.prototype.name==new RegExp.superConstructor }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("function X(x) { return new Object().__defineGetter__(0.2,function(){0.2}).length() }; X(void.isNull<<parseFloat(NaN))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("delete continue.toJSONProtocol.toLocaleString()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (continue.constructor.toObject() in true&&undef.toJSONProtocol) { String(0+break) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("import true.call(continue)>>break.toString()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("label escape(this) > Math.pow(new RegExp)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new {void}/IsSmi(new Object())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("switch (native==null?debugger.prototype.name:null.toLocaleString()) { case (NaN.push(this).join()): (break instanceof continue); break; }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = new Math.pow(x.push(0))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new (Array(NaN))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("label IsSmi(new RegExp).toLocaleString()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("label NaN.push(1).shift()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("{escape(undef),debugger.filter(0.2)}.prototype.-1 > new RegExp[0.2.valueOf()] = new RegExp.prototype.value.splice()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = new Join(0.2, x, continue, debugger, new Object()).size")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("with ({ Number(null).name() : Math.pow(true).__defineGetter__(debugger.toString(),function(){false+0.2}) }) { this.{x,break} }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Math.pow(goto debugger)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = IsPrimitive(void.pop())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = new Object().toString().toJSONProtocol")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("Instantiate(this.String(0.2))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("let ({-1.call(new RegExp)}) { break.length().splice() }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("import null.size.__defineGetter__(void.filter(x),function(){null.pop()})")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = new IsPrimitive(null.superConstructor)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new eval(-1.prototype.continue)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("switch (typeof(Iterator('a'))) { case (0.constructor>>~1): void.__defineGetter__(void,function(){1})/GetFunctionFor(0); break; }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for (false instanceof x.add(true.charAt(new RegExp)) in Join(undef.lastIndexOf(break), 0.2.add(new Object()), Iterator(1), {'a',x}, Array(new Object()))) { function () { null }/1&&-1 }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new escape('a'.concat(undef))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Function(Math.pow(NaN).toText)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = new throw new 0(NaN).className()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("delete String(GetFunctionFor(new Object()))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = Iterator(new Object()).charAt((0.2))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("Number(undef.charAt(1)).prototype.undef.lastIndexOf(true).slice(1.className(), undef.filter(-1)) = null<<null.push(parseInt('a'))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = {Math.max(1),IsSmi(new Object())}")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("switch (new Object().exec(0).isNull) { case (escape(IsSmi(false))): false.toObject()-null.size; break; }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new 'a'.__defineSetter__(debugger,function(){false}).name()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = debugger?-1:0+true.prototype.1")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new {false instanceof continue,native.size}")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("GetFunctionFor(continue.__lookupGetter__(0.2)).prototype.Math.max(1.splice()) = true.__defineGetter__(undef,function(){NaN}).filter(String(new RegExp))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("null.size-1.toLocaleString().prototype.(this).shift() = GetFunctionFor(native.charAt(break))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("Instantiate((!null.indexOf(-1)))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = {break.sort()}")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new throw new debugger.splice()(this.__lookupGetter__(undef))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("unescape(x[native]).prototype.0.splice().-1.prototype.true = x.prototype.value.className()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("export x+true.length")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("export debugger.indexOf(-1).indexOf(true.constructor)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("for ({break}.exec(new Object().continue) in eval(0.2.charAt(new Object()))) { throw new null.length(null?break:-1) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = NaN.toLocaleString().toObject()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("function X(x) { return Math.pow(break+false) }; X(Join(true.add(new Object()), null[-1], new RegExp[true], NaN&&debugger, x.charAt(undef)))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("switch ((break).add(true.sort())) { case (undef.charAt(native).__defineGetter__(IsPrimitive(1),function(){NaN<<new RegExp})): -1.__defineSetter__(null,function(){-1}) > this.charCodeAt(this); break; }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("import return 0.2.length")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("continue.join().toText.prototype.Number(debugger).slice(new RegExp.-1, (NaN)) = function () { (!null) }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("export Number(break.__lookupGetter__(false))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Date(return null/x)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("export Number(undef).shift()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = 1[native]/this&true")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("delete typeof(debugger.unshift())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("import x.charAt(false)&-1>>x")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("if (null.toText.superConstructor) { typeof(-1).toString() }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("let (parseFloat(continue.superConstructor)) { 0.2.toText.prototype.value }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("label parseInt(IsSmi(null))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("delete new Object().valueOf().indexOf(true-x)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = new unescape(1.__defineGetter__(new Object(),function(){x}))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("let (undef.size.splice()) { 1.constructor.charCodeAt(0+'a') }")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("this.new RegExp.pop().prototype.eval(debugger).toJSONProtocol = unescape(continue).valueOf()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("const x = new this.new RegExp.indexOf(unescape(new Object()))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = new break instanceof false instanceof native.length()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("Instantiate(parseFloat(x).valueOf())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("label {escape(true),Math.max(null)}")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("'a'>>>=void.prototype.value.prototype.break.prototype.break.indexOf(0.className()) = (!this&native)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("import Number(NaN).push(IsSmi(break))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("export true.exec(void).toObject()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Function({'a',true}/eval(new Object()))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("label null.concat(null).toObject()")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("native {0.2.length,new RegExp.lastIndexOf(-1)}")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("function X(x) { return Math.max({0.2}) }; X(true.charCodeAt(null).add(new RegExp.name()))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("delete -1.lastIndex.length")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("new Function(0.2[1].call(true > break))")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("Instantiate('a'.toLocaleString().splice())")
} catch (e) { if (e.message.length > 0) { print (e.message); } };

try {
  eval("x = typeof(void&&void)")
} catch (e) { if (e.message.length > 0) { print (e.message); } };
