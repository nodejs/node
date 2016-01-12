//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var shouldBailout = false;
var runningJITtedCode = false;
var reuseObjects = false;
var randomGenerator = function(inputseed) {
    var seed = inputseed;
    return function() {
    // Robert Jenkins' 32 bit integer hash function.
    // This hash is public domain. (http://burtleburtle.net/bob/hash/integer.html)
    seed = ((seed + 0x7ed55d16) + (seed << 12))  & 0xffffffff;
    seed = ((seed ^ 0xc761c23c) ^ (seed >>> 19)) & 0xffffffff;
    seed = ((seed + 0x165667b1) + (seed << 5))   & 0xffffffff;
    seed = ((seed + 0xd3a2646c) ^ (seed << 9))   & 0xffffffff;
    seed = ((seed + 0xfd7046c5) + (seed << 3))   & 0xffffffff;
    seed = ((seed ^ 0xb55a4f09) ^ (seed >>> 16)) & 0xffffffff;
    return (seed & 0xfffffff) / 0x10000000;
    };
};;
var intArrayCreatorCount = 0;
function GenerateArray(seed, arrayType, arraySize, missingValuePercent, typeOfDeclaration) {
   Math.random = randomGenerator(seed);
   var result, codeToExecute, thisArrayName, maxMissingValues = Math.floor(arraySize * missingValuePercent), noOfMissingValuesAdded = 0;
   var contents = [];
   var isVarArray = arrayType == 'var';
   function IsMissingValue(allowedMissingValues) {
       return Math.floor(Math.random() * 100) < allowedMissingValues
   }

   thisArrayName = 'tempIntArr' + intArrayCreatorCount++;

   for (var arrayIndex = 0; arrayIndex < arraySize; arrayIndex++) {
       if (isVarArray && arrayIndex != 0) {
           arrayType = Math.floor(Math.random() * 100) < 50 ? 'int' : 'float';
       }

       if(noOfMissingValuesAdded < maxMissingValues && IsMissingValue(missingValuePercent)) {
           noOfMissingValuesAdded++;
           contents.push('');
       } else {
           var randomValueGenerated;
           if (arrayType == 'int') {
               randomValueGenerated = Math.floor(Math.random() * seed);
           } else if (arrayType == 'float') {
               randomValueGenerated = Math.random() * seed;
           } else if (arrayType == 'var') {
               randomValueGenerated = '\'' + (Math.random() * seed).toString(36) + '\'';
           }
           contents.push(randomValueGenerated);
       }
   }

   if(contents.length == 1 && typeOfDeclaration == 'constructor') {
       contents.push(Math.floor(Math.random() * seed));
   }
   if(typeOfDeclaration == 'literal') {
       codeToExecute = 'var ' + thisArrayName + ' = [' + contents.join() + '];';
   } else {
       codeToExecute = 'var ' + thisArrayName + ' = new Array(' + contents.join() + ');';
   }

   codeToExecute += 'result =  ' + thisArrayName + ';';
   eval(codeToExecute);
   return result;
}
;

function getRoundValue(n) {
  if(n % 1 == 0) // int number
    return n % 2147483647;
  else // float number
    return n.toFixed(8);
 return n;
};

var print = WScript.Echo;
WScript.Echo = function(n) { if(!n) print(n); else print(formatOutput(n.toString())); };

function formatOutput(n) {{
 return n.replace(/[-+]?[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?/g, function(match) {{return getRoundValue(parseFloat(match));}} );
}};
function sumOfArrayElements(prev, curr, index, array) {
    return (typeof prev == "number" && typeof curr == "number") ? curr + prev : 0
}
;
var __counter = 0;
function test0(){
  var loopInvariant = shouldBailout ? 12 : 10;
  var GiantPrintArray = [];
  __counter++;;
  function makeArrayLength(x) { if(x < 1 || x > 4294967295 || x != x || isNaN(x) || !isFinite(x)) return 100; else return Math.floor(x) & 0xffff; };;
  function leaf() { return 100; };
  var obj0 = {};
  var arrObj0 = {};
  var litObj0 = {prop1: 3.14159265358979};
  var arrObj0 = {};
  var func0 = function(){
    d = (++ obj0.prop0);
    d = ((test0.caller) - ((a - a) * (4294967295, a) - 2108033774));
    obj0.prop0 = (- ((- -1414475580) * (test0.caller) - (p ^ 1595238567.1)));
    return leaf.call(arrObj0 );
  };
  var func1 = function(){
    a <<=func0.call(aliasOfobj0 );
    (function(argMath0, argObj1, argMath2){
      d |=(obj0.prop0 = (- k));
      d <<=(typeof(a)  != 'string') ;
      a = ((! func0.call(aliasOfobj0 )) * (test0.caller) - (((a === p)&&(k >= a)) - (- argObj1.prop0)));
      obj0.prop0 =157225836;
      a -=((- (-2067422549 >>> -1636353152.9)) >> ((d &= argObj1.length) * (-2067422549 >>> -1636353152.9) + (k + k)));
    })(p,arrObj0,(~ 2147483647));
    this.prop0=(- (test0.caller));
    return (- (ary.push((k * p + 552020892)))
);
  };
  var func2 = function(){
    return -125;
  };
  var func3 = function(argArr3,argObj4,argObj5,argArrObj6){
    a = func1.call(argObj4 );
    obj0.prop0 +=(argArr3.unshift(153045429, func1.call(argObj4 ), (! (typeof(-1410448513)  == 'string') ), (argArr3.reverse()), (test0.caller), ((-4.64733614747003E+18, p) & func1.call(argObj4 )), (argArr3.reverse()), (! (-2080272368.9 >> -1073741824)), (- (test0.caller)), (test0.caller), (! (-2080272368.9 >> -1073741824)), ((p ^ argObj4.prop0) + (- 153)), obj0.prop0, ((a ^ k) * ((test0.caller) - 1178228778.1))));
    d = (- ((obj0.prop0 -= k) * (-8.20260457466577E+18 + (- k))));
    return (leaf.call(arrObj0 ) + (- (p + -9.50122264758276E+16)));
  };
  var func4 = function(){
    aliasOfobj0.prop4=a;
    aliasOfobj0.prop4 -=(~ (-- aliasOfobj0.prop4));
    var uniqobj0 = [''];
    var uniqobj1 = uniqobj0[__counter%uniqobj0.length];
    uniqobj1.toString();
    return (test0.caller);
  };
  obj0.method0 = func0;
  obj0.method1 = obj0.method0;
  arrObj0.method0 = obj0.method1;
  arrObj0.method1 = obj0.method1;
  var ary = new Array(10);
  var i8 = new Int8Array(256);
  var i16 = new Int16Array(256);
  var i32 = new Int32Array(256);
  var ui8 = new Uint8Array(256);
  var ui16 = new Uint16Array(256);
  var ui32 = new Uint32Array(256);
  var f32 = new Float32Array(256);
  var f64 = new Float64Array(256);
  var uic8 = new Uint8ClampedArray(256);
  var IntArr0 = [];
  var IntArr1 = new Array(968398749,213,-2147483647);
  var FloatArr0 = [31,-65142194,688927696,251979442];
  var VarArr0 = new Array(arrObj0,-28,580282652,74274812,615692735,498133449,-2,-170);
  var a = 7.18833214492778E+17;
  var b = 2;
  var c = -7.40943319632701E+18;
  var d = -163914637;
  var e = 1767600395;
  var f = 3.67258722953737E+18;
  var g = 5.20382937941989E+18;
  var h = 57;
  var i = -9.90282939910018E+17;
  var j = -156825025;
  var k = -1179658900.9;
  var l = 243;
  var m = 1266801686;
  var n = -9.00984088258652E+18;
  var o = -3.18287498070513E+18;
  var p = -4.64827628003843E+18;
  var q = -2147483649;
  var r = -1118727838.9;
  arrObj0[0] = 6.36672072818147E+18;
  arrObj0[1] = 135;
  arrObj0[2] = -929029250;
  arrObj0[3] = 1245041367;
  arrObj0[4] = -5.26701184552884E+18;
  arrObj0[5] = 2146462342.1;
  arrObj0[6] = 2.94930445871878E+16;
  arrObj0[7] = -10;
  arrObj0[8] = -2147483647;
  arrObj0[9] = -227;
  arrObj0[10] = 71865530;
  arrObj0[11] = 2147483647;
  arrObj0[12] = -1918718448.9;
  arrObj0[13] = 2.88409576756109E+17;
  arrObj0[14] = -796492977;
  arrObj0[arrObj0.length-1] = 315591605.1;
  arrObj0.length = makeArrayLength(-2147483647);
  ary[0] = 180224827;
  ary[1] = -252;
  ary[2] = 43;
  ary[3] = 761200964;
  ary[4] = -149924584;
  ary[5] = 357066928;
  ary[6] = -1029007725;
  ary[7] = 65536;
  ary[8] = -104;
  ary[9] = -1584655723.9;
  ary[10] = 2147483650;
  ary[11] = -1850992516.9;
  ary[12] = -1.73690935785342E+18;
  ary[13] = 194;
  ary[14] = -220;
  ary[ary.length-1] = 1047975887;
  ary.length = makeArrayLength(1022565544.1);
  var aliasOfobj0 = obj0;;
  var aliasOfVarArr0 = VarArr0;;
  this.prop0 = 186;
  obj0.prop0 = 170;
  obj0.length = makeArrayLength(4294967297);
  aliasOfobj0.prop0 = 4.1347939449402E+18;
  aliasOfobj0.length = makeArrayLength(639922501);
  arrObj0.prop0 = -103;
  arrObj0.length = makeArrayLength(0);
  aliasOfobj0.prop4 = -430662160.9;
  IntArr0[IntArr0.length] = 774590552;
  IntArr0[6] = 215;
  IntArr0[0] = 8.2967774148005E+18;
  IntArr0[5] = 54835619;
  IntArr0[4] = 1;
  IntArr0[2] = 1003589260;
  IntArr0[3] = -85;
  IntArr1[5] = -1389274251;
  IntArr1[4] = 3;
  IntArr1[3] = 60;
  f = (typeof(k)  != 'undefined') ;
  var uniqobj2 = [arrObj0, obj0, obj0, obj0];
  var uniqobj3 = uniqobj2[__counter%uniqobj2.length];
  uniqobj3.method0();
  d = (p - (test0.caller));
  a +=(((leaf.call(arrObj0 ) + (- (p + -9.50122264758276E+16))) | (a * (k + k))) >> (a ^= (typeof(k)  != 'number') ));
  obj0.prop0 -=(typeof(p)  != 'boolean') ;
  aliasOfobj0.prop4 = (typeof(a)  == 'boolean') ;
  a +=(typeof(k)  != 'object') ;
  aliasOfobj0.prop4 = ((((leaf.call(arrObj0 ) + (- (p + -9.50122264758276E+16))) | (a * (k + k))) >> (a ^= (typeof(k)  != 'number') )) ^ (typeof(a)  == 'boolean') );
  obj0.prop0 = ((- (-657539943 << 114)) | (typeof(k)  == 'undefined') );
  obj0.prop0 -=(- ((k == k)||(a !== p)));
  if(((- ((k != a)&&(k <= p))) === ((k << k) * ((obj0.prop0 -= p) - (obj0.prop0 += -4.93175485795449E+18))))) {
    a = p;
    obj0.method0.call(obj0 );
    function func5 () {
      this.prop0 = (ary.push())
;
    }
    var uniqobj4 = new func5();
    d -=(332129343 >> (- (ary.push())
));
    uniqobj4.prop1=func4;
    obj0.prop0 = arrObj0.method0.call(obj0 );
ary0 = arguments;
    obj0.prop0 >>=(((-- d)) & arrObj0.method0.call(obj0 ));
    obj0.prop3 = (typeof(k)  == 'string') ;
    d = obj0.method1.call(uniqobj4 );
    arrObj0.method1.call(obj0 );
    var uniqobj5 = [''];
    uniqobj5[__counter%uniqobj5.length].toString();
    a +=((IntArr1.shift()) - 51);
    obj0.prop4 = ((-- d) * ((~ (- -710887009)) + (~ obj0.method0.call(aliasOfobj0 ))));
    uniqobj4.prop0 =((f32[(49) & 255] + (typeof(m)  != 'number') ), (- (~ k)));
    d >>=((obj0.prop0 > l)||(uniqobj4.prop0 == obj0.prop4));
    obj0.prop0 &=func0.call(aliasOfobj0 );
    aliasOfobj0.prop0 <<=(~ k);
  }
  else {
    d = (obj0.prop0 -= ((test0.caller) | -232));
    a <<=810624396;
    a -=(((test0.caller) & (IntArr1.reverse())) * (func1.call(litObj0 ) + (- e)));
    a = (((typeof(aliasOfobj0.prop4)  != null)  * (-240 - d) + (a |= p)) + (j === f));
    d |=(test0.caller);
    obj0.prop0 |=(-- d);
    a -=obj0.prop0;
    obj0.prop0 -=(test0.caller);
    d = (77784693 + ((- -1710585.9) + (obj0.prop0 + 1777722759.1)));
    a >>=((typeof(f)  == 'undefined')  * ((- (~ 4294967297)) + (typeof(f)  == 'string') ));
    this.prop2 = (((f + f) * ((d++ ) + (232 * f + obj0.prop0))) | (- (~ arrObj0.length)));
    litObj0.prop0={15: (~ arrObj0.length), 18: (parseInt("-0x72016C28") * (obj0.prop0 - (3.25814039531148E+18 - 880653562))), prop0: 166, prop1: ((~ 809498557), (f === arrObj0.length), (test0.caller), (arrObj0.length * (obj0.prop0 + f)), (- 92), (2147483647 - obj0.prop0)), prop2: (- (~ arrObj0.length)), prop3: ((typeof(arrObj0.length)  == 'undefined')  + (- obj0.prop0)), prop5: (-863285653 + (- -80)), prop6: (parseInt("-0x72016C28") * (obj0.prop0 - (3.25814039531148E+18 - 880653562)))};
    arrObj0.prop3 = ((- (typeof(c)  == 'number') ) - (arrObj0.length <= f));
    litObj0.prop0.prop1 +=obj0.method0.call(this );
    arrObj0.prop3 = (~ (- 4294967295));
    m = (~ (- 4294967295));
    m = (((- -242) - (c * 2025680025.1 + c)) + (func3.call(aliasOfobj0 , VarArr0, aliasOfobj0, litObj0, arrObj0) ^ (842191728 < arrObj0.length)));
    litObj0.prop0.prop6 &=((test0.caller) - (-- litObj0.prop0.prop2));
    var uniqobj6 = [''];
    uniqobj6[__counter%uniqobj6.length].toString();
  }
  g +=(- ((k != a)&&(k <= p)));
  this.prop3 = ((((c == c)&&(c === arrObj0.length)) + (201 + 963281167)) - 5.91645921888607E+18);
  litObj0.prop2 = ((test0.caller) - (typeof(f)  == 'number') );
  var uniqobj7 = [''];
  var uniqobj8 = uniqobj7[__counter%uniqobj7.length];
  uniqobj8.toString();
  m <<=(f === obj0.prop0);
  arrObj0.prop0 = (((3 ^ arrObj0.length) * -9.01970240007522E+18 + (test0.caller)) - ((- f) + ((arrObj0.length > arrObj0.length)||(c == arrObj0.length))));
  g = ((c == c)&&(c < c));
  g ^=func4.call(arrObj0 );
  arrObj0.prop0 = (test0.caller);
  WScript.Echo('a = ' + (a|0));
  WScript.Echo('b = ' + (b|0));
  WScript.Echo('c = ' + (c|0));
  WScript.Echo('d = ' + (d|0));
  WScript.Echo('e = ' + (e|0));
  WScript.Echo('f = ' + (f|0));
  WScript.Echo('g = ' + (g|0));
  WScript.Echo('h = ' + (h|0));
  WScript.Echo('i = ' + (i|0));
  WScript.Echo('j = ' + (j|0));
  WScript.Echo('k = ' + (k|0));
  WScript.Echo('l = ' + (l|0));
  WScript.Echo('m = ' + (m|0));
  WScript.Echo('n = ' + (n|0));
  WScript.Echo('o = ' + (o|0));
  WScript.Echo('p = ' + (p|0));
  WScript.Echo('q = ' + (q|0));
  WScript.Echo('r = ' + (r|0));
  WScript.Echo('this.prop0 = ' + (this.prop0|0));
  WScript.Echo('obj0.prop0 = ' + (obj0.prop0|0));
  WScript.Echo('obj0.length = ' + (obj0.length|0));
  WScript.Echo('aliasOfobj0.prop0 = ' + (aliasOfobj0.prop0|0));
  WScript.Echo('aliasOfobj0.length = ' + (aliasOfobj0.length|0));
  WScript.Echo('arrObj0.prop0 = ' + (arrObj0.prop0|0));
  WScript.Echo('arrObj0.length = ' + (arrObj0.length|0));
  WScript.Echo('aliasOfobj0.prop4 = ' + (aliasOfobj0.prop4|0));
  WScript.Echo('this.prop2 = ' + (this.prop2|0));
  WScript.Echo('this.prop3 = ' + (this.prop3|0));
  WScript.Echo('litObj0.prop2 = ' + (litObj0.prop2|0));
  WScript.Echo('arrObj0[0] = ' + (arrObj0[0]|0));
  WScript.Echo('arrObj0[1] = ' + (arrObj0[1]|0));
  WScript.Echo('arrObj0[2] = ' + (arrObj0[2]|0));
  WScript.Echo('arrObj0[3] = ' + (arrObj0[3]|0));
  WScript.Echo('arrObj0[4] = ' + (arrObj0[4]|0));
  WScript.Echo('arrObj0[5] = ' + (arrObj0[5]|0));
  WScript.Echo('arrObj0[6] = ' + (arrObj0[6]|0));
  WScript.Echo('arrObj0[7] = ' + (arrObj0[7]|0));
  WScript.Echo('arrObj0[8] = ' + (arrObj0[8]|0));
  WScript.Echo('arrObj0[9] = ' + (arrObj0[9]|0));
  WScript.Echo('arrObj0[10] = ' + (arrObj0[10]|0));
  WScript.Echo('arrObj0[11] = ' + (arrObj0[11]|0));
  WScript.Echo('arrObj0[12] = ' + (arrObj0[12]|0));
  WScript.Echo('arrObj0[13] = ' + (arrObj0[13]|0));
  WScript.Echo('arrObj0[14] = ' + (arrObj0[14]|0));
  WScript.Echo('arrObj0[arrObj0.length-1] = ' + (arrObj0[arrObj0.length-1]|0));
  WScript.Echo('arrObj0.length = ' + (arrObj0.length|0));
  WScript.Echo('ary[0] = ' + (ary[0]|0));
  WScript.Echo('ary[1] = ' + (ary[1]|0));
  WScript.Echo('ary[2] = ' + (ary[2]|0));
  WScript.Echo('ary[3] = ' + (ary[3]|0));
  WScript.Echo('ary[4] = ' + (ary[4]|0));
  WScript.Echo('ary[5] = ' + (ary[5]|0));
  WScript.Echo('ary[6] = ' + (ary[6]|0));
  WScript.Echo('ary[7] = ' + (ary[7]|0));
  WScript.Echo('ary[8] = ' + (ary[8]|0));
  WScript.Echo('ary[9] = ' + (ary[9]|0));
  WScript.Echo('ary[10] = ' + (ary[10]|0));
  WScript.Echo('ary[11] = ' + (ary[11]|0));
  WScript.Echo('ary[12] = ' + (ary[12]|0));
  WScript.Echo('ary[13] = ' + (ary[13]|0));
  WScript.Echo('ary[14] = ' + (ary[14]|0));
  WScript.Echo('ary[ary.length-1] = ' + (ary[ary.length-1]|0));
  WScript.Echo('ary.length = ' + (ary.length|0));
  for(var i =0;i<GiantPrintArray.length;i++){ 
  WScript.Echo(GiantPrintArray[i]); 
  };
  WScript.Echo('sumOfary = ' + ary.slice(0, 23).reduce(function(prev, curr) {{ return prev + curr; }},0));
  WScript.Echo('subset_of_ary = ' + ary.slice(0, 11));;
  WScript.Echo('sumOfIntArr0 = ' + IntArr0.slice(0, 23).reduce(function(prev, curr) {{ return prev + curr; }},0));
  WScript.Echo('subset_of_IntArr0 = ' + IntArr0.slice(0, 11));;
  WScript.Echo('sumOfIntArr1 = ' + IntArr1.slice(0, 23).reduce(function(prev, curr) {{ return prev + curr; }},0));
  WScript.Echo('subset_of_IntArr1 = ' + IntArr1.slice(0, 11));;
  WScript.Echo('sumOfFloatArr0 = ' + FloatArr0.slice(0, 23).reduce(function(prev, curr) {{ return prev + curr; }},0));
  WScript.Echo('subset_of_FloatArr0 = ' + FloatArr0.slice(0, 11));;
  WScript.Echo('sumOfVarArr0 = ' + VarArr0.slice(0, 23).reduce(function(prev, curr) {{ return prev + curr; }},0));
  WScript.Echo('subset_of_VarArr0 = ' + VarArr0.slice(0, 11));;
  WScript.Echo('sumOfaliasOfVarArr0 = ' + aliasOfVarArr0.slice(0, 23).reduce(function(prev, curr) {{ return prev + curr; }},0));
  WScript.Echo('subset_of_aliasOfVarArr0 = ' + aliasOfVarArr0.slice(0, 11));;
};

// generate profile
test0();
// Run Simple JIT
test0();
test0();

// run JITted code
runningJITtedCode = true;
test0();
