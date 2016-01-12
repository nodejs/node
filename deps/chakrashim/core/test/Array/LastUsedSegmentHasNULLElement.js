//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var runningJITtedCode = false;

var GiantPrintArray = [];
var reuseObjects = false;
var PolymorphicFuncObjArr = [];
var PolyFuncArr = [];
function GetPolymorphicFunction()
{
    if(PolyFuncArr.length > 1 )
    {
        var myFunc = PolyFuncArr.shift();
        PolyFuncArr.push(myFunc);
        return myFunc;
    }
    else
    {
        return PolyFuncArr[0];
    }
}
function GetObjectwithPolymorphicFunction(){
    if (reuseObjects)
    {
        if(PolymorphicFuncObjArr.length > 1 )
        {
            var myFunc = PolymorphicFuncObjArr.shift();
            PolymorphicFuncObjArr.push(myFunc);
            return myFunc
        }
        else
        {
            return PolymorphicFuncObjArr[0];
        }
    }
    else
    {
        var obj = {};
        obj.polyfunc = GetPolymorphicFunction();
        PolymorphicFuncObjArr.push(obj)
        return obj
    }
};
function InitPolymorphicFunctionArray(args)
{
    PolyFuncArr = [];
    for(var i=0;i<args.length;i++)
    {
        PolyFuncArr.push(args[i])
    }
}
;
function test0(){
  var obj0 = {};
  var obj1 = {};
  var arrObj0 = {};
  var func0 = function(){
  }
  var func1 = function(argObj0,argMath1,argStr2){
  }
  obj0.method0 = func0;
  obj1.method0 = func0;
  var ui16 = new Uint16Array(256);
  var f64 = new Float64Array(256);
  var uic8 = new Uint8ClampedArray(256);
  var IntArr0 = new Array();
  var IntArr1 = 1;
  var FloatArr0 = [65535,850033435,65535,3.47357153766714E+18,-1014997735,7.30803686256937E+18];
  var VarArr0 = [strvar6,-313763887.9];
  var b = 1;
  var d = 1;
  var e = 1;
  var f = 1;
  var g = 1;
  var strvar4 = 1;
  var strvar6 = 1;
  FloatArr0[10] = 1;
  FloatArr0[6] = 1;
  function bar0 (argObj5,argObj6){
  }
  InitPolymorphicFunctionArray(new Array(bar0));
  var __polyobj = GetObjectwithPolymorphicFunction();
  var strvar9 = 'ë*+»!' + '!j!##$c9';
  strvar9 = strvar9.substring((strvar9.length)/4,(strvar9.length)/1);
  var func4 = (argMath7,argFunc8) => {
}
    (IntArr0.unshift(b, (arrObj0.prop0 ? Math.floor(i8[(arrObj0.prop0) & 255]) : (new func0()).prop1 ), ((VarArr0.unshift((/a/ instanceof ((typeof RegExp == 'function' ) ? RegExp : Object)), FloatArr0[(14)], (new __polyobj.polyfunc(obj0,obj0)).prop0 , (new __polyobj.polyfunc(obj1,obj1)).prop1 , obj0.length, -5.58142230095393E+18, obj1.method0.call(arrObj0 ), g, obj1.method0.call(arrObj0 ), func0.call(obj0 ), ((FloatArr0[(14)] * (ui16[(obj0.prop1) & 255] + (obj0.prop0 ? -1 : -1.37703981324473E+18))) * ((parseInt("4044012402201", 5) * obj0.method0.call(arrObj0 ) - ((new Object()) instanceof ((typeof String == 'function' ) ? String : Object))) + obj1.method0())), (2147483647 * (((new Object()) instanceof ((typeof String == 'function' ) ? String : Object)) - arrObj0[(((((FloatArr0[(14)] * (ui16[(obj0.prop1) & 255] + (obj0.prop0 ? -1 : -1.37703981324473E+18))) * ((parseInt("4044012402201", 5) * obj0.method0.call(arrObj0 ) - ((new Object()) instanceof ((typeof String == 'function' ) ? String : Object))) + obj1.method0())) >= 0 ? ((FloatArr0[(14)] * (ui16[(obj0.prop1) & 255] + (obj0.prop0 ? -1 : -1.37703981324473E+18))) * ((parseInt("4044012402201", 5) * obj0.method0.call(arrObj0 ) - ((new Object()) instanceof ((typeof String == 'function' ) ? String : Object))) + obj1.method0())) : 0)) & 0XF)])), (2.16363795434958E+17 instanceof ((typeof RegExp == 'function' ) ? RegExp : Object)))) * ((typeof(d)  != null)  + (d != f))), IntArr1[(((((f &= ((arrObj0.prop1 <= -679080040) <= (-2147483647))) & ((-2147483649 >= f64[(f) & 255]) ? (arrObj0.prop1 <= -679080040) : bar0.call(arrObj0 , obj0, obj1))) >= 0 ? ((f &= ((arrObj0.prop1 <= -679080040) <= (-2147483647))) & ((-2147483649 >= f64[(f) & 255]) ? (arrObj0.prop1 <= -679080040) : bar0.call(arrObj0 , obj0, obj1))) : 0)) & 0XF)], ('ë*+»!' + '!j!##$c9'.indexOf('9' + '(!,*')), 2.42377081442484E+18, (-106 * (typeof (obj1.length != obj0.prop1)) - 1.98981237618569E+18), (arrObj0.length = (f < obj1.length)), (uic8[((typeof(obj0.prop0)  != 'object') ) & 255] ^ -1917286370.9), ((obj0.prop1 = obj0.prop1) * (('#'.indexOf(strvar4)) > (arrObj0.length !== e)) + (obj1.method0.call(obj0 ) !== func1.call(obj1 , obj1, func0.call(arrObj0 ), strvar9))), (('#'.indexOf(strvar4)) * (~ ((new EvalError()) instanceof ((typeof Object == 'function' ) ? Object : Object))) + ((~ strvar4) ? arrObj0.prop0 : (arrObj0.length > obj1.prop0))), (arrObj0.length ^= (typeof(d)  == 'number') )));

};

// generate profile
test0();
// Run Simple JIT
test0();
test0();

// run JITted code
runningJITtedCode = true;
test0();
WScript.Echo("Pass");
