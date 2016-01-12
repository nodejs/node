//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function echo(str)
{
  if(typeof(WScript) == "undefined")
  {
      print(str);
  }
  else
  {
      WScript.Echo(str);
  }
}

function f() { f(); }

var finally_ = 0;

try {

    try {
        try {

            f(); /* first SO */
        } finally {
            finally_++;

            try {
                f(); /* nested SO. This finally block is ignored. */
            } finally {
                finally_++;
            }
        }

    } catch (ex) {

        /* we should be able to raise 2 more SOs from here */
        try {
            f(); /* first SO */
        } finally {
            finally_++;

            try {
                f(); /* second SO. This finally block is ignored. */
            } finally {
                finally_++;
            }
        }

    }

} catch (ex) {
    echo(finally_ == 4 ? "PASS" : "FAIL");
}
