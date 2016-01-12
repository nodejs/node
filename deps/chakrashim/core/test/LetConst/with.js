//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// 0
{
with({x:0}) {
  WScript.Echo(x)
}
let x = 1
}

// 0
{
eval('with({x:0}) { WScript.Echo(x) }')
let x = 1
}

// 0
{
let f = function() {
  with({x:0}) {
    WScript.Echo(x)
  }
}
let x = 1
f()
}

// Reference error.
{
try {
  with({}) {
    WScript.Echo(x)
  }
  let x = 1
} catch(e) {
  WScript.Echo(e)
}
}

// Reference error.
{
try {
  eval('with({}) { WScript.Echo(x) }')
  let x = 1
} catch(e) {
  WScript.Echo(e)
}
}

// 1
{
with({x:0}) {
  let x = 1
  WScript.Echo(x)
}
}

// Reference error.
{
try { 
  with({x:0}) {
    WScript.Echo(x)
    let x = 1
  }
} catch(e) {
  WScript.Echo(e)
}
}

// string
with({x: 'x'})
{
    WScript.Echo(typeof x)
}
let x = 5