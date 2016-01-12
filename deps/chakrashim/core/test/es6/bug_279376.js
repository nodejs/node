//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

//OS 279376 : CheckinID:1304100:Exprgen:CAS:ARM::FRE:fatal error
//LowerCallArgs will handle the extraArgs. We only need to specify the position of the first argument
// i.e 1 and not 1 + extraArgs as done in other archs

var obj0 = {};
obj0.method0= function(){  };
protoObj0 = {__proto__ : obj0}

protoObj0.method1 = function() {
    this.method0.apply(this, arguments);
}

protoObj0.method1.prototype = {
        method0 : function(){
    },
}
protoObj0.method0.apply(new protoObj0.method1(...[1]));
WScript.Echo("pass");
