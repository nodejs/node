//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var obj0 = {};
var protoObj1 = {};
var func0 = function () {
  obj0.prop1;
};

var func2 = function () {
  obj0 = protoObj1;
  obj0.prop1 = 1;
};

func2();

Object.defineProperty(obj0, 'prop1', {
    get: function () {
      return 3;
    }
  }); 
func0();
delete obj0.prop1;

func2();
Object.create(protoObj1); 
func0();

WScript.Echo("PASSED");
