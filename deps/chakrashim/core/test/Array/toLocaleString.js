//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var echo = WScript.Echo;

function guarded_call(func) {
    try {
        func();
    } catch (e) {
        echo(e.name + " : " + e.message);
    }
}

var testCount = 0;
function scenario(title) {
    if (testCount > 0) {
        echo("\n");
    }
    echo((testCount++) + ".", title);
}

scenario("Non Object");
var nonObjList = [
    1, NaN, true, null, undefined
];
for (var i = 0; i < nonObjList.length; i++) {
    var o = nonObjList[i];
    guarded_call(function () {
        echo(o, "-->", Array.prototype.toLocaleString.apply(o));
    });
}

scenario("Object, length not uint32");
var badLength = [
    true, "abc", 1.234, {}
];
for (var i = 0; i < badLength.length; i++) {
    var len = badLength[i];
    var o = { length: len };
    guarded_call(function () {
        echo("length:", len, "-->", Array.prototype.toLocaleString.apply(o));
    });
}

scenario("Array: normal");
var o = [
    0, 1.23, NaN, true, "abc", {}, [], [0, 1, 2]
];
echo(Array.prototype.toLocaleString.apply(o));

scenario("Array: element toLocaleString not callable");
var o = [
    0,
    {toLocaleString: 123}
];
guarded_call(function () {
    echo(Array.prototype.toLocaleString.apply(o));
});

scenario("Array: element toLocaleString");
var o = [
    0,
    { toLocaleString: function () { return "anObject"; } },
    undefined,
    null,
    { toLocaleString: function () { return "another Object"; } },
    [
        1,
        { toLocaleString: function () { return "a 3rd Object"; } },
        2
    ]
];
echo(Array.prototype.toLocaleString.apply(o));


scenario("Object: normal");
var o = {
    0: 0,
    1: 1.23,
    2: NaN,
    3: true,
    4: "abc",
    5: {},
    6: [],
    7: [0, 1, 2],
    length: 8,
    8: "should not appear",
    "-1": "should not appear"
};
guarded_call(function () {
    echo(Array.prototype.toLocaleString.apply(o));
});

scenario("Object: element toLocaleString not callable");
var o = {
    0: 0,
    1: { toLocaleString: 123 },
    length: 2
};
guarded_call(function () {
    echo(Array.prototype.toLocaleString.apply(o));
});

scenario("Object: element toLocaleString");
var o = {
    0: 0,
    1: { toLocaleString: function () { return "anObject"; } },
    2: undefined,
    3: null,
    4: { toLocaleString: function () { return "another Object"; } },
    5: [
        1,
        { toLocaleString: function () { return "a 3rd Object"; } },
        2
    ],
    length: 6,
    6: "should not appear",
    7: "should not appear",
    "-1": "should not appear"
};
guarded_call(function () {
    echo(Array.prototype.toLocaleString.apply(o));
});

