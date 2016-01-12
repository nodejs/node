//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function echo() {
    var m = [].join.apply(arguments);
    if (this.WScript) { WScript.Echo(m); } else { console.log(m); }
}

echo("Big simple dictionary type handler -> Big ES5 array type handler");
(function(){
    var obj = [];
    for (var i = 0; i < 0x10001; i++) {
        obj["p" + i] = i;
    }

    echo(obj["p0"], obj["p65535"], obj["p65536"]);

    Object.defineProperty(obj, "0", {writable: false});
    echo(obj["p0"], obj["p65535"], obj["p65536"]);
})();

echo();
echo("Big dictionary type handler -> Big ES5 array type handler");
(function(){
    var obj = [];
    for (var i = 0; i < 0x10001; i++) {
        obj["p" + i] = i;
    }

    Object.defineProperty(obj, "p1", {get: function(){return "p1";}, configurable:true});
    echo(obj["p0"], obj["p65535"], obj["p65536"]);

    Object.defineProperty(obj, "0", {writable: false});
    echo(obj["p0"], obj["p65535"], obj["p65536"]);
})();

echo();
echo("Small ES5 array type handler -> Big ES5 array type handler");
(function(){
    var obj = [];
    Object.defineProperty(obj, "0", {get: function(){return "i0";}, configurable:true, enumerable:true});
    
    for (var i = 0; i < 0xFFFF; i++) {
        obj["p" + i] = i;
    }

    echo(obj[0], obj["p0"], obj["p65535"], obj["p65536"]);

    for (var i = 0xFFFF; i < 0x10010; i++) {
        obj["p" + i] = i;
    }
    echo(obj[0], obj["p0"], obj["p65535"], obj["p65536"]);
})();
