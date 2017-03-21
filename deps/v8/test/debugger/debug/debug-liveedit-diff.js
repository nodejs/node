// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


Debug = debug.Debug

function CheckCompareOneWay(s1, s2) {
  var diff_array = Debug.LiveEdit.TestApi.CompareStrings(s1, s2);

  var pos1 = 0;
  var pos2 = 0;
  print("Compare:");
  print("s1='" + s1 + "'");
  print("s2='" + s2 + "'");
  print("Diff:");
  print("" + diff_array);
  for (var i = 0; i < diff_array.length; i += 3) {
    var similar_length = diff_array[i] - pos1;
    assertEquals(s1.substring(pos1, pos1 + similar_length),
                 s2.substring(pos2, pos2 + similar_length));

    print(s1.substring(pos1, pos1 + similar_length));
    pos1 += similar_length;
    pos2 += similar_length;
    print("<<< " + pos1 + " " + diff_array[i + 1]);
    print(s1.substring(pos1, diff_array[i + 1]));
    print("===");
    print(s2.substring(pos2, diff_array[i + 2]));
    print(">>> " + pos2 + " " + diff_array[i + 2]);
    pos1 = diff_array[i + 1];
    pos2 = diff_array[i + 2];
  }
  {
    // After last change
    var similar_length = s1.length - pos1;
    assertEquals(similar_length, s2.length - pos2);
    assertEquals(s1.substring(pos1, pos1 + similar_length),
                 s2.substring(pos2, pos2 + similar_length));

    print(s1.substring(pos1, pos1 + similar_length));
  }
  print("");
}

function CheckCompareOneWayPlayWithLF(s1, s2) {
  var s1Oneliner = s1.replace(/\n/g, ' ');
  var s2Oneliner = s2.replace(/\n/g, ' ');
  CheckCompareOneWay(s1, s2);
  CheckCompareOneWay(s1Oneliner, s2);
  CheckCompareOneWay(s1, s2Oneliner);
  CheckCompareOneWay(s1Oneliner, s2Oneliner);
}

function CheckCompare(s1, s2) {
  CheckCompareOneWayPlayWithLF(s1, s2);
  CheckCompareOneWayPlayWithLF(s2, s1);
}

CheckCompare("", "");

CheckCompare("a", "b");

CheckCompare(
    "yesterday\nall\nmy\ntroubles\nseemed\nso\nfar\naway",
    "yesterday\nall\nmy\ntroubles\nseem\nso\nfar\naway"
);

CheckCompare(
    "yesterday\nall\nmy\ntroubles\nseemed\nso\nfar\naway",
    "\nall\nmy\ntroubles\nseemed\nso\nfar\naway"
);

CheckCompare(
    "yesterday\nall\nmy\ntroubles\nseemed\nso\nfar\naway",
    "all\nmy\ntroubles\nseemed\nso\nfar\naway"
);

CheckCompare(
    "yesterday\nall\nmy\ntroubles\nseemed\nso\nfar\naway",
    "yesterday\nall\nmy\ntroubles\nseemed\nso\nfar\naway\n"
);

CheckCompare(
    "yesterday\nall\nmy\ntroubles\nseemed\nso\nfar\naway",
    "yesterday\nall\nmy\ntroubles\nseemed\nso\n"
);
