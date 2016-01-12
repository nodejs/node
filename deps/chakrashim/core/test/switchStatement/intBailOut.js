//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function f(x,y)
{

                switch(x)
                {
                                case 1:

                                   WScript.Echo(1);
                                   break;
                                case 2:
                                                WScript.Echo(2);
                                   break;
                                case 3:
                                                WScript.Echo(3);
                                                break;
                                case 4:
                                                WScript.Echo(4);
                                                break;
                                case 'hello':         //string object
                                                WScript.Echo('hello');
                                                break;
                                case 5:
                                                WScript.Echo(5);
                                                break;
                                case 6:
                                                WScript.Echo(6);
                                                break;
                                case 7.1:               //float
                                                WScript.Echo(7);
                                                break;
                                case 8:
                                                WScript.Echo(8);
                                                break;
                                case 9:
                                                WScript.Echo(9);
                                                break;
                                default:
                                                WScript.Echo('default');
                                                break;
                }

                //This switch contains just integers and a object at the middle.
                switch(y)
                {
                                case 11:
                                   WScript.Echo(10);
                                   break;
                                case 12:
                                                WScript.Echo(12);
                                   break;
                                case 13:
                                                WScript.Echo(13);
                                                break;
                                case 14:
                                                WScript.Echo(14);
                                                break;
                                case f:   // object
                                                WScript.Echo(15);
                                                break;
                                case 16:
                                                WScript.Echo(16);
                                                break;
                                case 17:
                                                WScript.Echo(17);
                                                break;
                                case 18:
                                                WScript.Echo(18);
                                                break;
                                case 19:
                                                WScript.Echo(19);
                                                break;
                                case 20:
                                                WScript.Echo(20);
                                                break;
                                default:
                                                WScript.Echo('default');
                                                break;
                }
}

f(5,16);

for(i=0;i<15;i++)
{
    f(new Object,16);
}

/* Test case to see if TEST is not generated when type specialized to float type*/
function test(){
  var b = 1;
    for(i=0;i<1;i++){
     switch((b++)) {
      case 1:
        break;
      case 1:
        break;
      case 1:
        break;
      case 1:
        break;
      case 100:
        break;
      case 101:
        break;
      case 102:
        b =-2147483648; /*min int*/
        break;
    }
  }
};

//interpreter
test();

//jit
test();

