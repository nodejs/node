//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var GiantPrintArray = [];
function test0(){
  var obj0 = {};
  var a = 1;
  var d = 1;
  function func7 (argMath21){
    (function(){
    'use strict';;
      var v3291 = "I am global";
      var res;
      try {
          throw "I am not global"
      }catch(v3291){
             function foo(){return v3291;}
             res = foo();
           (function(argMath25){
            argMath21 =(-- argMath25);
          })(obj0.length);
           res = foo();
      }
      GiantPrintArray.push(res);
      
      
    })();
    function __objtypespecfoobar()
    {  
      obj0.v3293 = argMath21;
      for(var i in obj0)
      {
        GiantPrintArray.push(i + ":" + obj0[i]);
      }
      
    }
    __objtypespecfoobar();
    __objtypespecfoobar();
  }
  obj0[_strvar0] = 1; 
  var __loopvar4 = 0;
  for(var _strvar0 in obj0 ) {
    if(_strvar0.indexOf('method') != -1) continue;
    if(__loopvar4++ > 3) break;
    obj0[_strvar0] = func7(-- a); 
  }
  for(var i =0;i<GiantPrintArray.length;i++){ 
    WScript.Echo(GiantPrintArray[i]); 
  };
};

test0(); 
test0(); 
test0(); 
test0(); 
test0(); 
