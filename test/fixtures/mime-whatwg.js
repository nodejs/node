'use strict';

// TODO: Incorporate this with the other WPT tests if it makes sense to do that.

/* The following tests are copied from WPT. Modifications to them should be
   upstreamed first. Refs:
   https://github.com/w3c/web-platform-tests/blob/88b75886e/url/urltestdata.json
   License: http://www.w3.org/Consortium/Legal/2008/04-testsuite-copyright.html
*/
module.exports = [
  "Basics",
  {
    "input": "text/html;charset=gbk",
    "output": "text/html;charset=gbk",
    "navigable": true,
    "encoding": "GBK"
  },
  {
    "input": "TEXT/HTML;CHARSET=GBK",
    "output": "text/html;charset=GBK",
    "navigable": true,
    "encoding": "GBK"
  },
  "Legacy comment syntax",
  {
    "input": "text/html;charset=gbk(",
    "output": "text/html;charset=\"gbk(\"",
    "navigable": true,
    "encoding": null
  },
  {
    "input": "text/html;x=(;charset=gbk",
    "output": "text/html;x=\"(\";charset=gbk",
    "navigable": true,
    "encoding": "GBK"
  },
  "Duplicate parameter",
  {
    "input": "text/html;charset=gbk;charset=windows-1255",
    "output": "text/html;charset=gbk",
    "navigable": true,
    "encoding": "GBK"
  },
  {
    "input": "text/html;charset=();charset=GBK",
    "output": "text/html;charset=\"()\"",
    "navigable": true,
    "encoding": null
  },
  "Spaces",
  {
    "input": "text/html;charset =gbk",
    "output": "text/html",
    "navigable": true,
    "encoding": null
  },
  {
    "input": "text/html ;charset=gbk",
    "output": "text/html;charset=gbk",
    "navigable": true,
    "encoding": "GBK"
  },
  {
    "input": "text/html; charset=gbk",
    "output": "text/html;charset=gbk",
    "navigable": true,
    "encoding": "GBK"
  },
  {
    "input": "text/html;charset= gbk",
    "output": "text/html;charset=\" gbk\"",
    "navigable": true,
    "encoding": "GBK"
  },
  {
    "input": "text/html;charset= \"gbk\"",
    "output": "text/html;charset=\" \\\"gbk\\\"\"",
    "navigable": true,
    "encoding": null
  },
  "0x0B and 0x0C",
  {
    "input": "text/html;charset=\u000Bgbk",
    "output": "text/html",
    "navigable": true,
    "encoding": null
  },
  {
    "input": "text/html;charset=\u000Cgbk",
    "output": "text/html",
    "navigable": true,
    "encoding": null
  },
  {
    "input": "text/html;\u000Bcharset=gbk",
    "output": "text/html",
    "navigable": true,
    "encoding": null
  },
  {
    "input": "text/html;\u000Ccharset=gbk",
    "output": "text/html",
    "navigable": true,
    "encoding": null
  },
  "Single quotes are a token, not a delimiter",
  {
    "input": "text/html;charset='gbk'",
    "output": "text/html;charset='gbk'",
    "navigable": true,
    "encoding": null
  },
  {
    "input": "text/html;charset='gbk",
    "output": "text/html;charset='gbk",
    "navigable": true,
    "encoding": null
  },
  {
    "input": "text/html;charset=gbk'",
    "output": "text/html;charset=gbk'",
    "navigable": true,
    "encoding": null
  },
  {
    "input": "text/html;charset=';charset=GBK",
    "output": "text/html;charset='",
    "navigable": true,
    "encoding": null
  },
  "Invalid parameters",
  {
    "input": "text/html;test;charset=gbk",
    "output": "text/html;charset=gbk",
    "navigable": true,
    "encoding": "GBK"
  },
  {
    "input": "text/html;test=;charset=gbk",
    "output": "text/html;charset=gbk",
    "navigable": true,
    "encoding": "GBK"
  },
  {
    "input": "text/html;';charset=gbk",
    "output": "text/html;charset=gbk",
    "navigable": true,
    "encoding": "GBK"
  },
  {
    "input": "text/html;\";charset=gbk",
    "output": "text/html;charset=gbk",
    "navigable": true,
    "encoding": "GBK"
  },
  {
    "input": "text/html ; ; charset=gbk",
    "output": "text/html;charset=gbk",
    "navigable": true,
    "encoding": "GBK"
  },
  {
    "input": "text/html;;;;charset=gbk",
    "output": "text/html;charset=gbk",
    "navigable": true,
    "encoding": "GBK"
  },
  {
    "input": "text/html;charset= \"\u007F;charset=GBK",
    "output": "text/html;charset=GBK",
    "navigable": true,
    "encoding": "GBK"
  },
  {
    "input": "text/html;charset=\"\u007F;charset=foo\";charset=GBK",
    "output": "text/html;charset=GBK",
    "navigable": true,
    "encoding": "GBK"
  },
  "Double quotes",
  {
    "input": "text/html;charset=\"gbk\"",
    "output": "text/html;charset=gbk",
    "navigable": true,
    "encoding": "GBK"
  },
  {
    "input": "text/html;charset=\"gbk",
    "output": "text/html;charset=gbk",
    "navigable": true,
    "encoding": "GBK"
  },
  {
    "input": "text/html;charset=gbk\"",
    "output": "text/html;charset=\"gbk\\\"\"",
    "navigable": true,
    "encoding": null
  },
  {
    "input": "text/html;charset=\" gbk\"",
    "output": "text/html;charset=\" gbk\"",
    "navigable": true,
    "encoding": "GBK"
  },
  {
    "input": "text/html;charset=\"gbk \"",
    "output": "text/html;charset=\"gbk \"",
    "navigable": true,
    "encoding": "GBK"
  },
  {
    "input": "text/html;charset=\"\\ gbk\"",
    "output": "text/html;charset=\" gbk\"",
    "navigable": true,
    "encoding": "GBK"
  },
  {
    "input": "text/html;charset=\"\\g\\b\\k\"",
    "output": "text/html;charset=gbk",
    "navigable": true,
    "encoding": "GBK"
  },
  {
    "input": "text/html;charset=\"gbk\"x",
    "output": "text/html;charset=gbk",
    "navigable": true,
    "encoding": "GBK"
  },
  {
    "input": "text/html;charset=\"\";charset=GBK",
    "output": "text/html;charset=\"\"",
    "navigable": true,
    "encoding": null
  },
  {
    "input": "text/html;charset=\";charset=GBK",
    "output": "text/html;charset=\";charset=GBK\"",
    "navigable": true,
    "encoding": null
  },
  "Unexpected code points",
  {
    "input": "text/html;charset={gbk}",
    "output": "text/html;charset=\"{gbk}\"",
    "navigable": true,
    "encoding": null
  },
  "Parameter name longer than 127",
  {
    "input": "text/html;0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789=x;charset=gbk",
    "output": "text/html;0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789=x;charset=gbk",
    "navigable": true,
    "encoding": "GBK"
  },
  "type/subtype longer than 127",
  {
    "input": "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789/0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789",
    "output": "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789/0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
  },
  "Valid",
  {
    "input": "!#$%&'*+-.^_`|~0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz/!#$%&'*+-.^_`|~0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz;!#$%&'*+-.^_`|~0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz=!#$%&'*+-.^_`|~0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz",
    "output": "!#$%&'*+-.^_`|~0123456789abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz/!#$%&'*+-.^_`|~0123456789abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz;!#$%&'*+-.^_`|~0123456789abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz=!#$%&'*+-.^_`|~0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
  },
  {
    "input": "x/x;x=\"\t !\\\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\u0080\u0081\u0082\u0083\u0084\u0085\u0086\u0087\u0088\u0089\u008A\u008B\u008C\u008D\u008E\u008F\u0090\u0091\u0092\u0093\u0094\u0095\u0096\u0097\u0098\u0099\u009A\u009B\u009C\u009D\u009E\u009F\u00A0\u00A1\u00A2\u00A3\u00A4\u00A5\u00A6\u00A7\u00A8\u00A9\u00AA\u00AB\u00AC\u00AD\u00AE\u00AF\u00B0\u00B1\u00B2\u00B3\u00B4\u00B5\u00B6\u00B7\u00B8\u00B9\u00BA\u00BB\u00BC\u00BD\u00BE\u00BF\u00C0\u00C1\u00C2\u00C3\u00C4\u00C5\u00C6\u00C7\u00C8\u00C9\u00CA\u00CB\u00CC\u00CD\u00CE\u00CF\u00D0\u00D1\u00D2\u00D3\u00D4\u00D5\u00D6\u00D7\u00D8\u00D9\u00DA\u00DB\u00DC\u00DD\u00DE\u00DF\u00E0\u00E1\u00E2\u00E3\u00E4\u00E5\u00E6\u00E7\u00E8\u00E9\u00EA\u00EB\u00EC\u00ED\u00EE\u00EF\u00F0\u00F1\u00F2\u00F3\u00F4\u00F5\u00F6\u00F7\u00F8\u00F9\u00FA\u00FB\u00FC\u00FD\u00FE\u00FF\"",
    "output": "x/x;x=\"\t !\\\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\u0080\u0081\u0082\u0083\u0084\u0085\u0086\u0087\u0088\u0089\u008A\u008B\u008C\u008D\u008E\u008F\u0090\u0091\u0092\u0093\u0094\u0095\u0096\u0097\u0098\u0099\u009A\u009B\u009C\u009D\u009E\u009F\u00A0\u00A1\u00A2\u00A3\u00A4\u00A5\u00A6\u00A7\u00A8\u00A9\u00AA\u00AB\u00AC\u00AD\u00AE\u00AF\u00B0\u00B1\u00B2\u00B3\u00B4\u00B5\u00B6\u00B7\u00B8\u00B9\u00BA\u00BB\u00BC\u00BD\u00BE\u00BF\u00C0\u00C1\u00C2\u00C3\u00C4\u00C5\u00C6\u00C7\u00C8\u00C9\u00CA\u00CB\u00CC\u00CD\u00CE\u00CF\u00D0\u00D1\u00D2\u00D3\u00D4\u00D5\u00D6\u00D7\u00D8\u00D9\u00DA\u00DB\u00DC\u00DD\u00DE\u00DF\u00E0\u00E1\u00E2\u00E3\u00E4\u00E5\u00E6\u00E7\u00E8\u00E9\u00EA\u00EB\u00EC\u00ED\u00EE\u00EF\u00F0\u00F1\u00F2\u00F3\u00F4\u00F5\u00F6\u00F7\u00F8\u00F9\u00FA\u00FB\u00FC\u00FD\u00FE\u00FF\""
  },
  "End-of-file handling",
  {
    "input": "x/x;test",
    "output": "x/x"
  },
  {
    "input": "x/x;test=\"\\",
    "output": "x/x;test=\"\\\\\""
  },
  "Whitespace (not handled by generated-mime-types.json or above)",
  {
    "input": "x/x;x= ",
    "output": "x/x"
  },
  {
    "input": "x/x;x=\t",
    "output": "x/x"
  },
  {
    "input": "x/x\n\r\t ;x=x",
    "output": "x/x;x=x"
  },
  {
    "input": "\n\r\t x/x;x=x\n\r\t ",
    "output": "x/x;x=x"
  },
  {
    "input": "x/x;\n\r\t x=x\n\r\t ;x=y",
    "output": "x/x;x=x"
  },
  "Latin1",
  {
    "input": "text/html;test=\u00FF;charset=gbk",
    "output": "text/html;test=\"\u00FF\";charset=gbk",
    "navigable": true,
    "encoding": "GBK"
  },
  ">Latin1",
  {
    "input": "x/x;test=\uFFFD;x=x",
    "output": "x/x;x=x"
  },
  "Failure",
  {
    "input": "\u000Bx/x",
    "output": null
  },
  {
    "input": "\u000Cx/x",
    "output": null
  },
  {
    "input": "x/x\u000B",
    "output": null
  },
  {
    "input": "x/x\u000C",
    "output": null
  },
  {
    "input": "",
    "output": null
  },
  {
    "input": "\t",
    "output": null
  },
  {
    "input": "/",
    "output": null
  },
  {
    "input": "bogus",
    "output": null
  },
  {
    "input": "bogus/",
    "output": null
  },
  {
    "input": "bogus/ ",
    "output": null
  },
  {
    "input": "bogus/bogus/;",
    "output": null
  },
  {
    "input": "</>",
    "output": null
  },
  {
    "input": "(/)",
    "output": null
  },
  {
    "input": "ÿ/ÿ",
    "output": null
  },
  {
    "input": "text/html(;doesnot=matter",
    "output": null
  },
  {
    "input": "{/}",
    "output": null
  },
  {
    "input": "\u0100/\u0100",
    "output": null
  },
  {
    "input": "text /html",
    "output": null
  },
  {
    "input": "text/ html",
    "output": null
  },
  {
    "input": "\"text/html\"",
    "output": null
  }
];
