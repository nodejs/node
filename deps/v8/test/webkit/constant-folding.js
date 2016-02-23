// Copyright 2013 the V8 project authors. All rights reserved.
// Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1.  Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
// 2.  Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
// ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

description(
"This test checks some cases that might be affected by constant folding."
);

shouldBe('"abc" + "2.1"', '"abc2.1"');
shouldBe('"123" + "2.1"', '"1232.1"');
shouldBe('"123" + "="', '"123="');
shouldBe('"*" + "123"', '"*123"');

shouldBe('!"abc"', 'false');
shouldBe('!""', 'true');

shouldBe('10.3 + 2.1', '12.4');
shouldBe('10.3 + "2.1"', '"10.32.1"');
shouldBe('"10.3" + 2.1 ', '"10.32.1"');
shouldBe('"10.3" + "2.1"', '"10.32.1"');
shouldBe('10.3 + true', '11.3');
shouldBe('"10.3" + true', '"10.3true"');
shouldBe('10.3 + false', '10.3');
shouldBe('"10.3" + false', '"10.3false"');
shouldBe('true + 2.1', '3.1');
shouldBe('true + "2.1"', '"true2.1"');
shouldBe('false + 2.1', '2.1');
shouldBe('false + "2.1"', '"false2.1"');

shouldBe('10.3 - 2.1', '8.200000000000001');
shouldBe('10.3 - "2.1"', '8.200000000000001');
shouldBe('"10.3" - 2.1 ', '8.200000000000001');
shouldBe('"10.3" - "2.1"', '8.200000000000001');
shouldBe('10.3 - true', '9.3');
shouldBe('"10.3" - true', '9.3');
shouldBe('10.3 - false', '10.3');
shouldBe('"10.3" - false', '10.3');
shouldBe('true - 2.1', '-1.1');
shouldBe('true - "2.1"', '-1.1');
shouldBe('false - 2.1', '-2.1');
shouldBe('false - "2.1"', '-2.1');

shouldBe('10.3 * 2.1', '21.630000000000003');
shouldBe('10.3 * "2.1"', '21.630000000000003');
shouldBe('"10.3" * 2.1', '21.630000000000003');
shouldBe('"10.3" * "2.1"', '21.630000000000003');
shouldBe('10.3 * true', '10.3');
shouldBe('"10.3" * true', '10.3');
shouldBe('10.3 * false', '0');
shouldBe('"10.3" * false', '0');
shouldBe('true * 10.3', '10.3');
shouldBe('true * "10.3"', '10.3');
shouldBe('false * 10.3', '0');
shouldBe('false * "10.3"', '0');

shouldBe('10.3 / 2', '5.15');
shouldBe('"10.3" / 2', '5.15');
shouldBe('10.3 / "2"', '5.15');
shouldBe('"10.3" / "2"', '5.15');
shouldBe('10.3 / true', '10.3');
shouldBe('"10.3" / true', '10.3');
shouldBe('true / 2', '0.5');
shouldBe('true / "2"', '0.5');
shouldBe('false / 2', '0');
shouldBe('false / "2"', '0');

shouldBe('10.3 % 2.1', '1.9000000000000004');
shouldBe('"10.3" % 2.1', '1.9000000000000004');
shouldBe('10.3 % "2.1"', '1.9000000000000004');
shouldBe('"10.3" % "2.1"', '1.9000000000000004');
shouldBe('10.3 % true', '0.3000000000000007');
shouldBe('"10.3" % true', '0.3000000000000007');
shouldBe('true % 2', '1');
shouldBe('true % "2"', '1');
shouldBe('false % 2', '0');
shouldBe('false % "2"', '0');

shouldBe('10.3 << 2.1', '40');
shouldBe('"10.3" << 2.1', '40');
shouldBe('10.3 << "2.1"', '40');
shouldBe('"10.3" << "2.1"', '40');
shouldBe('10.3 << true', '20');
shouldBe('"10.3" << true', '20');
shouldBe('10.3 << false', '10');
shouldBe('"10.3" << false', '10');
shouldBe('true << 2.1', '4');
shouldBe('true << "2.1"', '4');
shouldBe('false << 2.1', '0');
shouldBe('false << "2.1"', '0');

shouldBe('10.3 >> 2.1', '2');
shouldBe('"10.3" >> 2.1', '2');
shouldBe('10.3 >> "2.1"', '2');
shouldBe('"10.3" >> "2.1"', '2');
shouldBe('10.3 >> true', '5');
shouldBe('"10.3" >> true', '5');
shouldBe('10.3 >> false', '10');
shouldBe('"10.3" >> false', '10');
shouldBe('true >> 2.1', '0');
shouldBe('true >> "2.1"', '0');
shouldBe('false >> 2.1', '0');
shouldBe('false >> "2.1"', '0');

shouldBe('-10.3 >>> 2.1', '1073741821');
shouldBe('"-10.3">>> 2.1', '1073741821');
shouldBe('-10.3 >>> "2.1"', '1073741821');
shouldBe('"-10.3">>> "2.1"', '1073741821');
shouldBe('-10.3 >>> true', '2147483643');
shouldBe('"-10.3">>> true', '2147483643');
shouldBe('-10.3 >>> false', '4294967286');
shouldBe('"-10.3" >>> false', '4294967286');
shouldBe('true >>> 2.1', '0');
shouldBe('true >>> "2.1"', '0');
shouldBe('false >>> 2.1', '0');
shouldBe('false >>> "2.1"', '0');


shouldBe('10.3 & 3.1', '2');
shouldBe('"10.3" & 3.1', '2');
shouldBe('10.3 & "3.1"', '2');
shouldBe('"10.3" & "3.1"', '2');
shouldBe('10.3 & true', '0');
shouldBe('"10.3" & true', '0');
shouldBe('11.3 & true', '1');
shouldBe('"11.3" & true', '1');
shouldBe('10.3 & false', '0');
shouldBe('"10.3" & false', '0');
shouldBe('11.3 & false', '0');
shouldBe('"11.3" & false', '0');
shouldBe('true & 3.1', '1');
shouldBe('true & "3.1"', '1');
shouldBe('true & 2.1', '0');
shouldBe('true & "2.1"', '0');
shouldBe('false & 3.1', '0');
shouldBe('false & "3.1"', '0');
shouldBe('false & 2.1', '0');
shouldBe('false & "2.1"', '0');


shouldBe('10.3 | 3.1', '11');
shouldBe('"10.3" | 3.1', '11');
shouldBe('10.3 | "3.1"', '11');
shouldBe('"10.3" | "3.1"', '11');
shouldBe('10.3 | true', '11');
shouldBe('"10.3" | true', '11');
shouldBe('11.3 | true', '11');
shouldBe('"11.3" | true', '11');
shouldBe('10.3 | false', '10');
shouldBe('"10.3" | false', '10');
shouldBe('11.3 | false', '11');
shouldBe('"11.3" | false', '11');
shouldBe('true | 3.1', '3');
shouldBe('true | "3.1"', '3');
shouldBe('true | 2.1', '3');
shouldBe('true | "2.1"', '3');
shouldBe('false | 3.1', '3');
shouldBe('false | "3.1"', '3');
shouldBe('false | 2.1', '2');
shouldBe('false | "2.1"', '2');

shouldBe('10.3 ^ 3.1', '9');
shouldBe('"10.3" ^ 3.1', '9');
shouldBe('10.3 ^ "3.1"', '9');
shouldBe('"10.3" ^ "3.1"', '9');
shouldBe('10.3 ^ true', '11');
shouldBe('"10.3" ^ true', '11');
shouldBe('11.3 ^ true', '10');
shouldBe('"11.3" ^ true', '10');
shouldBe('10.3 ^ false', '10');
shouldBe('"10.3" ^ false', '10');
shouldBe('11.3 ^ false', '11');
shouldBe('"11.3" ^ false', '11');
shouldBe('true ^ 3.1', '2');
shouldBe('true ^ "3.1"', '2');
shouldBe('true ^ 2.1', '3');
shouldBe('true ^ "2.1"', '3');
shouldBe('false ^ 3.1', '3');
shouldBe('false ^ "3.1"', '3');
shouldBe('false ^ 2.1', '2');
shouldBe('false ^ "2.1"', '2');

shouldBe('10.3 == 3.1', 'false');
shouldBe('3.1 == 3.1', 'true');
shouldBe('"10.3" == 3.1', 'false');
shouldBe('"3.1" == 3.1', 'true');
shouldBe('10.3 == "3.1"', 'false');
shouldBe('3.1 == "3.1"', 'true');
shouldBe('"10.3" == "3.1"', 'false');
shouldBe('"3.1" == "3.1"', 'true');
shouldBe('10.3 == true', 'false');
shouldBe('1 == true', 'true');
shouldBe('"10.3" == true', 'false');
shouldBe('"1" == true', 'true');
shouldBe('10.3 == false', 'false');
shouldBe('0 == false', 'true');
shouldBe('"10.3" == false', 'false');
shouldBe('"0" == false', 'true');
shouldBe('true == 3.1', 'false');
shouldBe('true == 1', 'true');
shouldBe('true == "3.1"', 'false');
shouldBe('true == "1" ', 'true');
shouldBe('false == 3.1', 'false');
shouldBe('false == 0', 'true');
shouldBe('false == "3.1"', 'false');
shouldBe('false == "0"', 'true');
shouldBe('true == true', 'true');
shouldBe('false == true', 'false');
shouldBe('true == false', 'false');
shouldBe('false == false', 'true');

shouldBe('10.3 != 3.1', 'true');
shouldBe('3.1 != 3.1', 'false');
shouldBe('"10.3" != 3.1', 'true');
shouldBe('"3.1" != 3.1', 'false');
shouldBe('10.3 != "3.1"', 'true');
shouldBe('3.1 != "3.1"', 'false');
shouldBe('"10.3" != "3.1"', 'true');
shouldBe('"3.1" != "3.1"', 'false');
shouldBe('10.3 != true', 'true');
shouldBe('1 != true', 'false');
shouldBe('"10.3" != true', 'true');
shouldBe('"1" != true', 'false');
shouldBe('10.3 != false', 'true');
shouldBe('0 != false', 'false');
shouldBe('"10.3" != false', 'true');
shouldBe('"0" != false', 'false');
shouldBe('true != 3.1', 'true');
shouldBe('true != 1', 'false');
shouldBe('true != "3.1"', 'true');
shouldBe('true != "1" ', 'false');
shouldBe('false != 3.1', 'true');
shouldBe('false != 0', 'false');
shouldBe('false != "3.1"', 'true');
shouldBe('false != "0"', 'false');
shouldBe('true != true', 'false');
shouldBe('false != true', 'true');
shouldBe('true != false', 'true');
shouldBe('false != false', 'false');

shouldBe('10.3 > 3.1', 'true');
shouldBe('3.1 > 3.1', 'false');
shouldBe('"10.3" > 3.1', 'true');
shouldBe('"3.1" > 3.1', 'false');
shouldBe('10.3 > "3.1"', 'true');
shouldBe('3.1 > "3.1"', 'false');
shouldBe('"10.3" > "3.1"', 'false');
shouldBe('"3.1" > "3.1"', 'false');
shouldBe('10.3 > true', 'true');
shouldBe('0 > true', 'false');
shouldBe('"10.3" > true', 'true');
shouldBe('"0" > true', 'false');
shouldBe('10.3 > false', 'true');
shouldBe('-1 > false', 'false');
shouldBe('"10.3" > false', 'true');
shouldBe('"-1" > false', 'false');
shouldBe('true > 0.1', 'true');
shouldBe('true > 1.1', 'false');
shouldBe('true > "0.1"', 'true');
shouldBe('true > "1.1"', 'false');
shouldBe('false > -3.1', 'true');
shouldBe('false > 0', 'false');
shouldBe('false > "-3.1"', 'true');
shouldBe('false > "0"', 'false');
shouldBe('true > true', 'false');
shouldBe('false > true', 'false');
shouldBe('true > false', 'true');
shouldBe('false > false', 'false');

shouldBe('10.3 < 3.1', 'false');
shouldBe('2.1 < 3.1', 'true');
shouldBe('"10.3" < 3.1', 'false');
shouldBe('"2.1" < 3.1', 'true');
shouldBe('10.3 < "3.1"', 'false');
shouldBe('2.1 < "3.1"', 'true');
shouldBe('"10.3" < "3.1"', 'true');
shouldBe('"2.1" < "3.1"', 'true');
shouldBe('10.3 < true', 'false');
shouldBe('0 < true', 'true');
shouldBe('"10.3" < true', 'false');
shouldBe('"0" < true', 'true');
shouldBe('10.3 < false', 'false');
shouldBe('-1 < false', 'true');
shouldBe('"10.3" < false', 'false');
shouldBe('"-1" < false', 'true');
shouldBe('true < 0.1', 'false');
shouldBe('true < 1.1', 'true');
shouldBe('true < "0.1"', 'false');
shouldBe('true < "1.1"', 'true');
shouldBe('false < -3.1', 'false');
shouldBe('false < 0.1', 'true');
shouldBe('false < "-3.1"', 'false');
shouldBe('false < "0.1"', 'true');
shouldBe('true < true', 'false');
shouldBe('false < true', 'true');
shouldBe('true < false', 'false');
shouldBe('false < false', 'false');

shouldBe('10.3 >= 3.1', 'true');
shouldBe('2.1 >= 3.1', 'false');
shouldBe('"10.3" >= 3.1', 'true');
shouldBe('"2.1" >= 3.1', 'false');
shouldBe('10.3 >= "3.1"', 'true');
shouldBe('2.1 >= "3.1"', 'false');
shouldBe('"10.3" >= "3.1"', 'false');
shouldBe('"2.1" >= "3.1"', 'false');
shouldBe('10.3 >= true', 'true');
shouldBe('0 >= true', 'false');
shouldBe('"10.3" >= true', 'true');
shouldBe('"0" >= true', 'false');
shouldBe('10.3 >= false', 'true');
shouldBe('-1 >= false', 'false');
shouldBe('"10.3" >= false', 'true');
shouldBe('"-1" >= false', 'false');
shouldBe('true >= 0.1', 'true');
shouldBe('true >= 1.1', 'false');
shouldBe('true >= "0.1"', 'true');
shouldBe('true >= "1.1"', 'false');
shouldBe('false >= -3.1', 'true');
shouldBe('false >= 0', 'true');
shouldBe('false >= "-3.1"', 'true');
shouldBe('false >= "0"', 'true');
shouldBe('true >= true', 'true');
shouldBe('false >= true', 'false');
shouldBe('true >= false', 'true');
shouldBe('false >= false', 'true');

shouldBe('10.3 <= 3.1', 'false');
shouldBe('2.1 <= 3.1', 'true');
shouldBe('"10.3" <= 3.1', 'false');
shouldBe('"2.1" <= 3.1', 'true');
shouldBe('10.3 <= "3.1"', 'false');
shouldBe('2.1 <= "3.1"', 'true');
shouldBe('"10.3" <= "3.1"', 'true');
shouldBe('"2.1" <= "3.1"', 'true');
shouldBe('10.3 <= true', 'false');
shouldBe('0 <= true', 'true');
shouldBe('"10.3" <= true', 'false');
shouldBe('"0" <= true', 'true');
shouldBe('10.3 <= false', 'false');
shouldBe('-1 <= false', 'true');
shouldBe('"10.3" <= false', 'false');
shouldBe('"-1" <= false', 'true');
shouldBe('true <= 0.1', 'false');
shouldBe('true <= 1.1', 'true');
shouldBe('true <= "0.1"', 'false');
shouldBe('true <= "1.1"', 'true');
shouldBe('false <= -3.1', 'false');
shouldBe('false <= 0.1', 'true');
shouldBe('false <= "-3.1"', 'false');
shouldBe('false <= "0.1"', 'true');
shouldBe('true <= true', 'true');
shouldBe('false <= true', 'true');
shouldBe('true <= false', 'false');
shouldBe('false <= false', 'true');

shouldBe('true && true', 'true');
shouldBe('true && false', 'false');
shouldBe('false && true', 'false');
shouldBe('false && false', 'false');
shouldBe('1.1 && true', 'true');
shouldBe('1.1 && false', 'false');
shouldBe('0 && true', '0');
shouldBe('0 && false', '0');
shouldBe('"1.1" && true', 'true');
shouldBe('"1.1" && false', 'false');
shouldBe('"0" && true', 'true');
shouldBe('"0" && false', 'false');
shouldBe('true && 1.1', '1.1');
shouldBe('true && 0', '0');
shouldBe('false && 1.1', 'false');
shouldBe('false && 0', 'false');
shouldBe('true && "1.1"', '"1.1"');
shouldBe('true && "0"', '"0"');
shouldBe('false && "1.1"', 'false');
shouldBe('false && "0"', 'false');
shouldBe('1.1 && 1.1', '1.1');
shouldBe('1.1 && 0', '0');
shouldBe('0 && 1.1', '0');
shouldBe('0 && 0', '0');
shouldBe('"1.1" && 1.1', '1.1');
shouldBe('"1.1" && 0', '0');
shouldBe('"0" && 1.1', '1.1');
shouldBe('"0" && 0', '0');
shouldBe('1.1 && "1.1"', '"1.1"');
shouldBe('1.1 && "0"', '"0"');
shouldBe('0 && "1.1"', '0');
shouldBe('0 && "0"', '0');
shouldBe('"1.1" && "1.1"', '"1.1"');
shouldBe('"1.1" && "0"', '"0"');
shouldBe('"0" && "1.1"', '"1.1"');
shouldBe('"0" && "0"', '"0"');

shouldBe('true || true', 'true');
shouldBe('true || false', 'true');
shouldBe('false || true', 'true');
shouldBe('false || false', 'false');
shouldBe('1.1 || true', '1.1');
shouldBe('1.1 || false', '1.1');
shouldBe('0 || true', 'true');
shouldBe('0 || false', 'false');
shouldBe('"1.1" || true', '"1.1"');
shouldBe('"1.1" || false', '"1.1"');
shouldBe('"0" || true', '"0"');
shouldBe('"0" || false', '"0"');
shouldBe('true || 1.1', 'true');
shouldBe('true || 0', 'true');
shouldBe('false || 1.1', '1.1');
shouldBe('false || 0', '0');
shouldBe('true || "1.1"', 'true');
shouldBe('true || "0"', 'true');
shouldBe('false || "1.1"', '"1.1"');
shouldBe('false || "0"', '"0"');
shouldBe('1.1 || 1.1', '1.1');
shouldBe('1.1 || 0', '1.1');
shouldBe('0 || 1.1', '1.1');
shouldBe('0 || 0', '0');
shouldBe('"1.1" || 1.1', '"1.1"');
shouldBe('"1.1" || 0', '"1.1"');
shouldBe('"0" || 1.1', '"0"');
shouldBe('"0" || 0', '"0"');
shouldBe('1.1 || "1.1"', '1.1');
shouldBe('1.1 || "0"', '1.1');
shouldBe('0 || "1.1"', '"1.1"');
shouldBe('0 || "0"', '"0"');
shouldBe('"1.1" || "1.1"', '"1.1"');
shouldBe('"1.1" || "0"', '"1.1"');
shouldBe('"0" || "1.1"', '"0"');
shouldBe('"0" || "0"', '"0"');

shouldBe('+3.1', '3.1');
shouldBe('+ +3.1', '3.1');
shouldBe('+"3.1"', '3.1');
shouldBe('+true', '1');
shouldBe('+false', '0');

shouldBe('-3.1', '-3.1');
shouldBe('- -3.1', '3.1');
shouldBe('-"3.1"', '-3.1');
shouldBe('-true', '-1');
shouldBe('-false', '-0');

shouldBe('~3', '-4');
shouldBe('~ ~3', '3');
shouldBe('~"3"', '-4');
shouldBe('~true', '-2');
shouldBe('~false', '-1');

shouldBe('!true', 'false');
shouldBe('!false', 'true');
shouldBe('!3', 'false');
shouldBe('!0', 'true');

shouldBe('10.3 / 0', 'Infinity');
shouldBe('"10.3" / 0', 'Infinity');
shouldBe('-10.3 / 0', '-Infinity');
shouldBe('"-10.3" / 0', '-Infinity');
shouldBe('true / 0', 'Infinity');
shouldBe('false / 0', 'NaN');
shouldBe('0 / 0', 'NaN');

shouldBe('10.3 / -0', '-Infinity');
shouldBe('"10.3" / -0', '-Infinity');
shouldBe('-10.3 / -0', 'Infinity');
shouldBe('"-10.3" / -0', 'Infinity');
shouldBe('true / -0', '-Infinity');
shouldBe('false / -0', 'NaN');
shouldBe('0 / -0', 'NaN');

shouldBe('1 / -0', '-Infinity');
shouldBe('1 / - 0', '-Infinity');
shouldBe('1 / - -0', 'Infinity');
shouldBe('1 / - - -0', '-Infinity');
