//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(args)
{
  WScript.Echo(args);
}

var foo = {};

foo.with =10;
write(foo.with);

foo.if = foo.with;
write(foo.if);

foo.function = function (){ write("Inside foo.function");}

function bar()
{
  this.function();
}
var k = bar.apply(foo);

var tokenCodes  = {
    null: 0,
    true: 1,
    false: 2,
    break:3,
    case:4,
    catch: function() { write("Inside tokenCodes.catch");}
    };

tokenCodes.catch();

for(var i in tokenCodes)
{
   write(i);
}

var allTokenCodes  = {

break : 1,
case : 1,
catch : 1,
class : 1,
const : 1,
continue : 1,
debugger : 1,
decimal : 1,
default : 1,
delete : 1,
do : 1,
else : 1,
enum : 1,
export : 1,
extends : 1,
false : 1,
finally : 1,
for : 1,
function : 1,
if : 1,
import : 1,
in : 1,
instanceof : 1,
invariant : 1,
long : 1,
namespace : 1,
native : 1,
new : 1,
null : 1,
package : 1,
private : 1,
return : 1,
sbyte : 1,
super : 1,
switch : 1,
this : 1,
throw : 1,
throws : 1,
transient : 1,
true : 1,
try : 1,
typeof : 1,
var : 1,
void : 1,
volatile : 1,
while : 1,
with : 1
}

for(var i in allTokenCodes)
{
 write(i);
}
