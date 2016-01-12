//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

 (function() {
              spc    = /^\s$/,
              range  = 0xFFFF;

          var fillZero = (function() {
              var tmp = [];
              return function(num, n) {
                  var nnum;
                  num += "";
                  if (num.length < n) {
                      if (!tmp[n]) {
                          z = tmp[n] = new Array(n).join('0');
                      } else {
                          z = tmp[n];
                      }
                      num  = z + num;
                      nnum = num.length
                      num  = num.substring(nnum-n, nnum);
                  }
                  return num;
              };
          })();

      WScript.Echo("start");

          for (r = 0x00; r < range; r++) {
              if (spc.test(String.fromCharCode(r))) {
                  WScript.Echo('\\u'+fillZero(r.toString(16), 4));
              }
          }
      })();
