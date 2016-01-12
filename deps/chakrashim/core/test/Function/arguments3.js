//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var echo = function(v) {
    WScript.Echo(v);
}

var get_args = function(a, b) {
    return arguments;
}

echo("--- toString test ---");
echo(get_args());
echo(get_args(1, '2', null));

echo("\n--- getOwnPropertyNames should enumerate all properties ---");
//
// TODO: Currently the following outputs 2 length properties.
//
echo(Object.getOwnPropertyNames(
     get_args()));
echo(Object.getOwnPropertyNames(
     get_args(1)));
echo(Object.getOwnPropertyNames(
     get_args(1, 2)));

var a = get_args(1, 2, '3', 'arg4', 'arg5');
echo(Object.getOwnPropertyNames(a));

delete a[0];
delete a[1];
delete a[4];
a[0] = 'arg0';
echo(Object.getOwnPropertyNames(a));
