# Copyright 2011 the V8 project authors. All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of Google Inc. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Tests of duplicate properties in object literals.

# ----------------------------------------------------------------------
# Utility functions to generate a number of tests for each property
# name pair.

def PropertyTest(name, propa, propb, allow_strict = True):
  replacement = {"id1": propa, "id2": propb, "name": name}

  # Tests same test in both strict and non-strict context.
  def StrictTest(name, source, replacement, expectation):
    if (allow_strict):
      Template("strict-" + name,
               "\"use strict\";\n" + source)(replacement, expectation)
      Template(name, source)(replacement, expectation)

  # This one only fails in non-strict context.
  if (allow_strict):
    Template("strict-$name-data-data", """
        "use strict";
        var o = {$id1: 42, $id2: 42};
      """)(replacement, "strict_duplicate_property")

  Template("$name-data-data", """
      var o = {$id1: 42, $id2: 42};
    """)(replacement, None)

  StrictTest("$name-data-get", """
      var o = {$id1: 42, get $id2(){}};
    """, replacement, "accessor_data_property")

  StrictTest("$name-data-set", """
      var o = {$id1: 42, set $id2(v){}};
    """, replacement, "accessor_data_property")

  StrictTest("$name-get-data", """
      var o = {get $id1(){}, $id2: 42};
    """, replacement, "accessor_data_property")

  StrictTest("$name-set-data", """
      var o = {set $id1(v){}, $id2: 42};
    """, replacement, "accessor_data_property")

  StrictTest("$name-get-get", """
      var o = {get $id1(){}, get $id2(){}};
    """, replacement, "accessor_get_set")

  StrictTest("$name-set-set", """
      var o = {set $id1(v){}, set $id2(v){}};
    """, replacement, "accessor_get_set")

  StrictTest("$name-nested-get", """
      var o = {get $id1(){}, o: {get $id2(){} } };
    """, replacement, None)

  StrictTest("$name-nested-set", """
      var o = {set $id1(){}, o: {set $id2(){} } };
    """, replacement, None)


def TestBothWays(name, propa, propb, allow_strict = True):
  PropertyTest(name + "-1", propa, propb, allow_strict)
  PropertyTest(name + "-2", propb, propa, allow_strict)

def TestSame(name, prop, allow_strict = True):
  PropertyTest(name, prop, prop, allow_strict)

#-----------------------------------------------------------------------

# Simple identifier property
TestSame("a", "a")

# Get/set identifiers
TestSame("get-id", "get")
TestSame("set-id", "set")

# Number properties
TestSame("0", "0")
TestSame("0.1", "0.1")
TestSame("1.0", "1.0")
TestSame("42.33", "42.33")
TestSame("2^32-2", "4294967294")
TestSame("2^32", "4294967296")
TestSame("2^53", "9007199254740992")
TestSame("Hex20", "0x20")
TestSame("exp10", "1e10")
TestSame("exp20", "1e20")
TestSame("Oct40", "040", False);


# String properties
TestSame("str-a", '"a"')
TestSame("str-0", '"0"')
TestSame("str-42", '"42"')
TestSame("str-empty", '""')

# Keywords
TestSame("if", "if")
TestSame("case", "case")

# Future reserved keywords
TestSame("public", "public")
TestSame("class", "class")


# Test that numbers are converted to string correctly.

TestBothWays("hex-int", "0x20", "32")
TestBothWays("oct-int", "040", "32", False)  # Octals disallowed in strict mode.
TestBothWays("dec-int", "32.00", "32")
TestBothWays("dec-underflow-int",
             "32.00000000000000000000000000000000000000001", "32")
TestBothWays("exp-int", "3.2e1", "32")
TestBothWays("exp-int", "3200e-2", "32")
TestBothWays("overflow-inf", "1e2000", "Infinity")
TestBothWays("overflow-inf-exact", "1.797693134862315808e+308", "Infinity")
TestBothWays("non-overflow-inf-exact", "1.797693134862315807e+308",
                                       "1.7976931348623157e+308")
TestBothWays("underflow-0", "1e-2000", "0")
TestBothWays("underflow-0-exact", "2.4703282292062E-324", "0")
TestBothWays("non-underflow-0-exact", "2.4703282292063E-324", "5e-324")
TestBothWays("precission-loss-high", "9007199254740992", "9007199254740993")
TestBothWays("precission-loss-low", "1.9999999999999998", "1.9999999999999997")
TestBothWays("non-canonical-literal-int", "1.0", "1")
TestBothWays("non-canonical-literal-frac", "1.50", "1.5")
TestBothWays("rounding-down", "1.12512512512512452", "1.1251251251251244")
TestBothWays("rounding-up", "1.12512512512512453", "1.1251251251251246")

TestBothWays("hex-int-str", "0x20", '"32"')
TestBothWays("dec-int-str", "32.00", '"32"')
TestBothWays("exp-int-str", "3.2e1", '"32"')
TestBothWays("overflow-inf-str", "1e2000", '"Infinity"')
TestBothWays("underflow-0-str", "1e-2000", '"0"')
TestBothWays("non-canonical-literal-int-str", "1.0", '"1"')
TestBothWays("non-canonical-literal-frac-str", "1.50", '"1.5"')
