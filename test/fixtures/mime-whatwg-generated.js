'use strict';

/* The following tests are copied from WPT. Modifications to them should be
   upstreamed first. Refs:
   https://github.com/w3c/web-platform-tests/blob/88b75886e/url/urltestdata.json
   License: http://www.w3.org/Consortium/Legal/2008/04-testsuite-copyright.html
*/
module.exports = [
  {
    "input": "\u0000/x",
    "output": null
  },
  {
    "input": "x/\u0000",
    "output": null
  },
  {
    "input": "x/x;\u0000=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0000;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\"\u0000\";bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "\u0001/x",
    "output": null
  },
  {
    "input": "x/\u0001",
    "output": null
  },
  {
    "input": "x/x;\u0001=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0001;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\"\u0001\";bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "\u0002/x",
    "output": null
  },
  {
    "input": "x/\u0002",
    "output": null
  },
  {
    "input": "x/x;\u0002=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0002;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\"\u0002\";bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "\u0003/x",
    "output": null
  },
  {
    "input": "x/\u0003",
    "output": null
  },
  {
    "input": "x/x;\u0003=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0003;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\"\u0003\";bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "\u0004/x",
    "output": null
  },
  {
    "input": "x/\u0004",
    "output": null
  },
  {
    "input": "x/x;\u0004=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0004;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\"\u0004\";bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "\u0005/x",
    "output": null
  },
  {
    "input": "x/\u0005",
    "output": null
  },
  {
    "input": "x/x;\u0005=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0005;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\"\u0005\";bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "\u0006/x",
    "output": null
  },
  {
    "input": "x/\u0006",
    "output": null
  },
  {
    "input": "x/x;\u0006=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0006;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\"\u0006\";bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "\u0007/x",
    "output": null
  },
  {
    "input": "x/\u0007",
    "output": null
  },
  {
    "input": "x/x;\u0007=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0007;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\"\u0007\";bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "\b/x",
    "output": null
  },
  {
    "input": "x/\b",
    "output": null
  },
  {
    "input": "x/x;\b=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\b;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\"\b\";bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "\t/x",
    "output": null
  },
  {
    "input": "x/\t",
    "output": null
  },
  {
    "input": "x/x;\t=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "\n/x",
    "output": null
  },
  {
    "input": "x/\n",
    "output": null
  },
  {
    "input": "x/x;\n=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\n;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\"\n\";bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "\u000b/x",
    "output": null
  },
  {
    "input": "x/\u000b",
    "output": null
  },
  {
    "input": "x/x;\u000b=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u000b;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\"\u000b\";bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "\f/x",
    "output": null
  },
  {
    "input": "x/\f",
    "output": null
  },
  {
    "input": "x/x;\f=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\f;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\"\f\";bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "\r/x",
    "output": null
  },
  {
    "input": "x/\r",
    "output": null
  },
  {
    "input": "x/x;\r=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\r;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\"\r\";bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "\u000e/x",
    "output": null
  },
  {
    "input": "x/\u000e",
    "output": null
  },
  {
    "input": "x/x;\u000e=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u000e;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\"\u000e\";bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "\u000f/x",
    "output": null
  },
  {
    "input": "x/\u000f",
    "output": null
  },
  {
    "input": "x/x;\u000f=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u000f;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\"\u000f\";bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "\u0010/x",
    "output": null
  },
  {
    "input": "x/\u0010",
    "output": null
  },
  {
    "input": "x/x;\u0010=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0010;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\"\u0010\";bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "\u0011/x",
    "output": null
  },
  {
    "input": "x/\u0011",
    "output": null
  },
  {
    "input": "x/x;\u0011=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0011;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\"\u0011\";bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "\u0012/x",
    "output": null
  },
  {
    "input": "x/\u0012",
    "output": null
  },
  {
    "input": "x/x;\u0012=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0012;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\"\u0012\";bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "\u0013/x",
    "output": null
  },
  {
    "input": "x/\u0013",
    "output": null
  },
  {
    "input": "x/x;\u0013=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0013;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\"\u0013\";bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "\u0014/x",
    "output": null
  },
  {
    "input": "x/\u0014",
    "output": null
  },
  {
    "input": "x/x;\u0014=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0014;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\"\u0014\";bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "\u0015/x",
    "output": null
  },
  {
    "input": "x/\u0015",
    "output": null
  },
  {
    "input": "x/x;\u0015=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0015;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\"\u0015\";bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "\u0016/x",
    "output": null
  },
  {
    "input": "x/\u0016",
    "output": null
  },
  {
    "input": "x/x;\u0016=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0016;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\"\u0016\";bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "\u0017/x",
    "output": null
  },
  {
    "input": "x/\u0017",
    "output": null
  },
  {
    "input": "x/x;\u0017=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0017;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\"\u0017\";bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "\u0018/x",
    "output": null
  },
  {
    "input": "x/\u0018",
    "output": null
  },
  {
    "input": "x/x;\u0018=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0018;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\"\u0018\";bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "\u0019/x",
    "output": null
  },
  {
    "input": "x/\u0019",
    "output": null
  },
  {
    "input": "x/x;\u0019=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0019;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\"\u0019\";bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "\u001a/x",
    "output": null
  },
  {
    "input": "x/\u001a",
    "output": null
  },
  {
    "input": "x/x;\u001a=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u001a;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\"\u001a\";bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "\u001b/x",
    "output": null
  },
  {
    "input": "x/\u001b",
    "output": null
  },
  {
    "input": "x/x;\u001b=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u001b;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\"\u001b\";bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "\u001c/x",
    "output": null
  },
  {
    "input": "x/\u001c",
    "output": null
  },
  {
    "input": "x/x;\u001c=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u001c;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\"\u001c\";bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "\u001d/x",
    "output": null
  },
  {
    "input": "x/\u001d",
    "output": null
  },
  {
    "input": "x/x;\u001d=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u001d;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\"\u001d\";bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "\u001e/x",
    "output": null
  },
  {
    "input": "x/\u001e",
    "output": null
  },
  {
    "input": "x/x;\u001e=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u001e;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\"\u001e\";bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "\u001f/x",
    "output": null
  },
  {
    "input": "x/\u001f",
    "output": null
  },
  {
    "input": "x/x;\u001f=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u001f;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\"\u001f\";bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": " /x",
    "output": null
  },
  {
    "input": "x/ ",
    "output": null
  },
  {
    "input": "x/x; =x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "\"/x",
    "output": null
  },
  {
    "input": "x/\"",
    "output": null
  },
  {
    "input": "x/x;\"=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "(/x",
    "output": null
  },
  {
    "input": "x/(",
    "output": null
  },
  {
    "input": "x/x;(=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=(;bonus=x",
    "output": "x/x;x=\"(\";bonus=x"
  },
  {
    "input": "x/x;x=\"(\";bonus=x",
    "output": "x/x;x=\"(\";bonus=x"
  },
  {
    "input": ")/x",
    "output": null
  },
  {
    "input": "x/)",
    "output": null
  },
  {
    "input": "x/x;)=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=);bonus=x",
    "output": "x/x;x=\")\";bonus=x"
  },
  {
    "input": "x/x;x=\")\";bonus=x",
    "output": "x/x;x=\")\";bonus=x"
  },
  {
    "input": ",/x",
    "output": null
  },
  {
    "input": "x/,",
    "output": null
  },
  {
    "input": "x/x;,=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=,;bonus=x",
    "output": "x/x;x=\",\";bonus=x"
  },
  {
    "input": "x/x;x=\",\";bonus=x",
    "output": "x/x;x=\",\";bonus=x"
  },
  {
    "input": "x/x;/=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=/;bonus=x",
    "output": "x/x;x=\"/\";bonus=x"
  },
  {
    "input": "x/x;x=\"/\";bonus=x",
    "output": "x/x;x=\"/\";bonus=x"
  },
  {
    "input": ":/x",
    "output": null
  },
  {
    "input": "x/:",
    "output": null
  },
  {
    "input": "x/x;:=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=:;bonus=x",
    "output": "x/x;x=\":\";bonus=x"
  },
  {
    "input": "x/x;x=\":\";bonus=x",
    "output": "x/x;x=\":\";bonus=x"
  },
  {
    "input": ";/x",
    "output": null
  },
  {
    "input": "x/;",
    "output": null
  },
  {
    "input": "</x",
    "output": null
  },
  {
    "input": "x/<",
    "output": null
  },
  {
    "input": "x/x;<=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=<;bonus=x",
    "output": "x/x;x=\"<\";bonus=x"
  },
  {
    "input": "x/x;x=\"<\";bonus=x",
    "output": "x/x;x=\"<\";bonus=x"
  },
  {
    "input": "=/x",
    "output": null
  },
  {
    "input": "x/=",
    "output": null
  },
  {
    "input": "x/x;x==;bonus=x",
    "output": "x/x;x=\"=\";bonus=x"
  },
  {
    "input": "x/x;x=\"=\";bonus=x",
    "output": "x/x;x=\"=\";bonus=x"
  },
  {
    "input": ">/x",
    "output": null
  },
  {
    "input": "x/>",
    "output": null
  },
  {
    "input": "x/x;>=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=>;bonus=x",
    "output": "x/x;x=\">\";bonus=x"
  },
  {
    "input": "x/x;x=\">\";bonus=x",
    "output": "x/x;x=\">\";bonus=x"
  },
  {
    "input": "?/x",
    "output": null
  },
  {
    "input": "x/?",
    "output": null
  },
  {
    "input": "x/x;?=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=?;bonus=x",
    "output": "x/x;x=\"?\";bonus=x"
  },
  {
    "input": "x/x;x=\"?\";bonus=x",
    "output": "x/x;x=\"?\";bonus=x"
  },
  {
    "input": "@/x",
    "output": null
  },
  {
    "input": "x/@",
    "output": null
  },
  {
    "input": "x/x;@=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=@;bonus=x",
    "output": "x/x;x=\"@\";bonus=x"
  },
  {
    "input": "x/x;x=\"@\";bonus=x",
    "output": "x/x;x=\"@\";bonus=x"
  },
  {
    "input": "[/x",
    "output": null
  },
  {
    "input": "x/[",
    "output": null
  },
  {
    "input": "x/x;[=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=[;bonus=x",
    "output": "x/x;x=\"[\";bonus=x"
  },
  {
    "input": "x/x;x=\"[\";bonus=x",
    "output": "x/x;x=\"[\";bonus=x"
  },
  {
    "input": "\\/x",
    "output": null
  },
  {
    "input": "x/\\",
    "output": null
  },
  {
    "input": "x/x;\\=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "]/x",
    "output": null
  },
  {
    "input": "x/]",
    "output": null
  },
  {
    "input": "x/x;]=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=];bonus=x",
    "output": "x/x;x=\"]\";bonus=x"
  },
  {
    "input": "x/x;x=\"]\";bonus=x",
    "output": "x/x;x=\"]\";bonus=x"
  },
  {
    "input": "{/x",
    "output": null
  },
  {
    "input": "x/{",
    "output": null
  },
  {
    "input": "x/x;{=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x={;bonus=x",
    "output": "x/x;x=\"{\";bonus=x"
  },
  {
    "input": "x/x;x=\"{\";bonus=x",
    "output": "x/x;x=\"{\";bonus=x"
  },
  {
    "input": "}/x",
    "output": null
  },
  {
    "input": "x/}",
    "output": null
  },
  {
    "input": "x/x;}=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=};bonus=x",
    "output": "x/x;x=\"}\";bonus=x"
  },
  {
    "input": "x/x;x=\"}\";bonus=x",
    "output": "x/x;x=\"}\";bonus=x"
  },
  {
    "input": "\u007f/x",
    "output": null
  },
  {
    "input": "x/\u007f",
    "output": null
  },
  {
    "input": "x/x;\u007f=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u007f;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\"\u007f\";bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "\u0080/x",
    "output": null
  },
  {
    "input": "x/\u0080",
    "output": null
  },
  {
    "input": "x/x;\u0080=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0080;bonus=x",
    "output": "x/x;x=\"\u0080\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u0080\";bonus=x",
    "output": "x/x;x=\"\u0080\";bonus=x"
  },
  {
    "input": "\u0081/x",
    "output": null
  },
  {
    "input": "x/\u0081",
    "output": null
  },
  {
    "input": "x/x;\u0081=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0081;bonus=x",
    "output": "x/x;x=\"\u0081\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u0081\";bonus=x",
    "output": "x/x;x=\"\u0081\";bonus=x"
  },
  {
    "input": "\u0082/x",
    "output": null
  },
  {
    "input": "x/\u0082",
    "output": null
  },
  {
    "input": "x/x;\u0082=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0082;bonus=x",
    "output": "x/x;x=\"\u0082\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u0082\";bonus=x",
    "output": "x/x;x=\"\u0082\";bonus=x"
  },
  {
    "input": "\u0083/x",
    "output": null
  },
  {
    "input": "x/\u0083",
    "output": null
  },
  {
    "input": "x/x;\u0083=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0083;bonus=x",
    "output": "x/x;x=\"\u0083\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u0083\";bonus=x",
    "output": "x/x;x=\"\u0083\";bonus=x"
  },
  {
    "input": "\u0084/x",
    "output": null
  },
  {
    "input": "x/\u0084",
    "output": null
  },
  {
    "input": "x/x;\u0084=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0084;bonus=x",
    "output": "x/x;x=\"\u0084\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u0084\";bonus=x",
    "output": "x/x;x=\"\u0084\";bonus=x"
  },
  {
    "input": "\u0085/x",
    "output": null
  },
  {
    "input": "x/\u0085",
    "output": null
  },
  {
    "input": "x/x;\u0085=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0085;bonus=x",
    "output": "x/x;x=\"\u0085\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u0085\";bonus=x",
    "output": "x/x;x=\"\u0085\";bonus=x"
  },
  {
    "input": "\u0086/x",
    "output": null
  },
  {
    "input": "x/\u0086",
    "output": null
  },
  {
    "input": "x/x;\u0086=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0086;bonus=x",
    "output": "x/x;x=\"\u0086\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u0086\";bonus=x",
    "output": "x/x;x=\"\u0086\";bonus=x"
  },
  {
    "input": "\u0087/x",
    "output": null
  },
  {
    "input": "x/\u0087",
    "output": null
  },
  {
    "input": "x/x;\u0087=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0087;bonus=x",
    "output": "x/x;x=\"\u0087\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u0087\";bonus=x",
    "output": "x/x;x=\"\u0087\";bonus=x"
  },
  {
    "input": "\u0088/x",
    "output": null
  },
  {
    "input": "x/\u0088",
    "output": null
  },
  {
    "input": "x/x;\u0088=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0088;bonus=x",
    "output": "x/x;x=\"\u0088\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u0088\";bonus=x",
    "output": "x/x;x=\"\u0088\";bonus=x"
  },
  {
    "input": "\u0089/x",
    "output": null
  },
  {
    "input": "x/\u0089",
    "output": null
  },
  {
    "input": "x/x;\u0089=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0089;bonus=x",
    "output": "x/x;x=\"\u0089\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u0089\";bonus=x",
    "output": "x/x;x=\"\u0089\";bonus=x"
  },
  {
    "input": "\u008a/x",
    "output": null
  },
  {
    "input": "x/\u008a",
    "output": null
  },
  {
    "input": "x/x;\u008a=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u008a;bonus=x",
    "output": "x/x;x=\"\u008a\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u008a\";bonus=x",
    "output": "x/x;x=\"\u008a\";bonus=x"
  },
  {
    "input": "\u008b/x",
    "output": null
  },
  {
    "input": "x/\u008b",
    "output": null
  },
  {
    "input": "x/x;\u008b=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u008b;bonus=x",
    "output": "x/x;x=\"\u008b\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u008b\";bonus=x",
    "output": "x/x;x=\"\u008b\";bonus=x"
  },
  {
    "input": "\u008c/x",
    "output": null
  },
  {
    "input": "x/\u008c",
    "output": null
  },
  {
    "input": "x/x;\u008c=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u008c;bonus=x",
    "output": "x/x;x=\"\u008c\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u008c\";bonus=x",
    "output": "x/x;x=\"\u008c\";bonus=x"
  },
  {
    "input": "\u008d/x",
    "output": null
  },
  {
    "input": "x/\u008d",
    "output": null
  },
  {
    "input": "x/x;\u008d=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u008d;bonus=x",
    "output": "x/x;x=\"\u008d\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u008d\";bonus=x",
    "output": "x/x;x=\"\u008d\";bonus=x"
  },
  {
    "input": "\u008e/x",
    "output": null
  },
  {
    "input": "x/\u008e",
    "output": null
  },
  {
    "input": "x/x;\u008e=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u008e;bonus=x",
    "output": "x/x;x=\"\u008e\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u008e\";bonus=x",
    "output": "x/x;x=\"\u008e\";bonus=x"
  },
  {
    "input": "\u008f/x",
    "output": null
  },
  {
    "input": "x/\u008f",
    "output": null
  },
  {
    "input": "x/x;\u008f=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u008f;bonus=x",
    "output": "x/x;x=\"\u008f\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u008f\";bonus=x",
    "output": "x/x;x=\"\u008f\";bonus=x"
  },
  {
    "input": "\u0090/x",
    "output": null
  },
  {
    "input": "x/\u0090",
    "output": null
  },
  {
    "input": "x/x;\u0090=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0090;bonus=x",
    "output": "x/x;x=\"\u0090\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u0090\";bonus=x",
    "output": "x/x;x=\"\u0090\";bonus=x"
  },
  {
    "input": "\u0091/x",
    "output": null
  },
  {
    "input": "x/\u0091",
    "output": null
  },
  {
    "input": "x/x;\u0091=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0091;bonus=x",
    "output": "x/x;x=\"\u0091\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u0091\";bonus=x",
    "output": "x/x;x=\"\u0091\";bonus=x"
  },
  {
    "input": "\u0092/x",
    "output": null
  },
  {
    "input": "x/\u0092",
    "output": null
  },
  {
    "input": "x/x;\u0092=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0092;bonus=x",
    "output": "x/x;x=\"\u0092\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u0092\";bonus=x",
    "output": "x/x;x=\"\u0092\";bonus=x"
  },
  {
    "input": "\u0093/x",
    "output": null
  },
  {
    "input": "x/\u0093",
    "output": null
  },
  {
    "input": "x/x;\u0093=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0093;bonus=x",
    "output": "x/x;x=\"\u0093\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u0093\";bonus=x",
    "output": "x/x;x=\"\u0093\";bonus=x"
  },
  {
    "input": "\u0094/x",
    "output": null
  },
  {
    "input": "x/\u0094",
    "output": null
  },
  {
    "input": "x/x;\u0094=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0094;bonus=x",
    "output": "x/x;x=\"\u0094\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u0094\";bonus=x",
    "output": "x/x;x=\"\u0094\";bonus=x"
  },
  {
    "input": "\u0095/x",
    "output": null
  },
  {
    "input": "x/\u0095",
    "output": null
  },
  {
    "input": "x/x;\u0095=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0095;bonus=x",
    "output": "x/x;x=\"\u0095\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u0095\";bonus=x",
    "output": "x/x;x=\"\u0095\";bonus=x"
  },
  {
    "input": "\u0096/x",
    "output": null
  },
  {
    "input": "x/\u0096",
    "output": null
  },
  {
    "input": "x/x;\u0096=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0096;bonus=x",
    "output": "x/x;x=\"\u0096\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u0096\";bonus=x",
    "output": "x/x;x=\"\u0096\";bonus=x"
  },
  {
    "input": "\u0097/x",
    "output": null
  },
  {
    "input": "x/\u0097",
    "output": null
  },
  {
    "input": "x/x;\u0097=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0097;bonus=x",
    "output": "x/x;x=\"\u0097\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u0097\";bonus=x",
    "output": "x/x;x=\"\u0097\";bonus=x"
  },
  {
    "input": "\u0098/x",
    "output": null
  },
  {
    "input": "x/\u0098",
    "output": null
  },
  {
    "input": "x/x;\u0098=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0098;bonus=x",
    "output": "x/x;x=\"\u0098\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u0098\";bonus=x",
    "output": "x/x;x=\"\u0098\";bonus=x"
  },
  {
    "input": "\u0099/x",
    "output": null
  },
  {
    "input": "x/\u0099",
    "output": null
  },
  {
    "input": "x/x;\u0099=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u0099;bonus=x",
    "output": "x/x;x=\"\u0099\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u0099\";bonus=x",
    "output": "x/x;x=\"\u0099\";bonus=x"
  },
  {
    "input": "\u009a/x",
    "output": null
  },
  {
    "input": "x/\u009a",
    "output": null
  },
  {
    "input": "x/x;\u009a=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u009a;bonus=x",
    "output": "x/x;x=\"\u009a\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u009a\";bonus=x",
    "output": "x/x;x=\"\u009a\";bonus=x"
  },
  {
    "input": "\u009b/x",
    "output": null
  },
  {
    "input": "x/\u009b",
    "output": null
  },
  {
    "input": "x/x;\u009b=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u009b;bonus=x",
    "output": "x/x;x=\"\u009b\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u009b\";bonus=x",
    "output": "x/x;x=\"\u009b\";bonus=x"
  },
  {
    "input": "\u009c/x",
    "output": null
  },
  {
    "input": "x/\u009c",
    "output": null
  },
  {
    "input": "x/x;\u009c=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u009c;bonus=x",
    "output": "x/x;x=\"\u009c\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u009c\";bonus=x",
    "output": "x/x;x=\"\u009c\";bonus=x"
  },
  {
    "input": "\u009d/x",
    "output": null
  },
  {
    "input": "x/\u009d",
    "output": null
  },
  {
    "input": "x/x;\u009d=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u009d;bonus=x",
    "output": "x/x;x=\"\u009d\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u009d\";bonus=x",
    "output": "x/x;x=\"\u009d\";bonus=x"
  },
  {
    "input": "\u009e/x",
    "output": null
  },
  {
    "input": "x/\u009e",
    "output": null
  },
  {
    "input": "x/x;\u009e=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u009e;bonus=x",
    "output": "x/x;x=\"\u009e\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u009e\";bonus=x",
    "output": "x/x;x=\"\u009e\";bonus=x"
  },
  {
    "input": "\u009f/x",
    "output": null
  },
  {
    "input": "x/\u009f",
    "output": null
  },
  {
    "input": "x/x;\u009f=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u009f;bonus=x",
    "output": "x/x;x=\"\u009f\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u009f\";bonus=x",
    "output": "x/x;x=\"\u009f\";bonus=x"
  },
  {
    "input": "\u00a0/x",
    "output": null
  },
  {
    "input": "x/\u00a0",
    "output": null
  },
  {
    "input": "x/x;\u00a0=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00a0;bonus=x",
    "output": "x/x;x=\"\u00a0\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00a0\";bonus=x",
    "output": "x/x;x=\"\u00a0\";bonus=x"
  },
  {
    "input": "\u00a1/x",
    "output": null
  },
  {
    "input": "x/\u00a1",
    "output": null
  },
  {
    "input": "x/x;\u00a1=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00a1;bonus=x",
    "output": "x/x;x=\"\u00a1\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00a1\";bonus=x",
    "output": "x/x;x=\"\u00a1\";bonus=x"
  },
  {
    "input": "\u00a2/x",
    "output": null
  },
  {
    "input": "x/\u00a2",
    "output": null
  },
  {
    "input": "x/x;\u00a2=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00a2;bonus=x",
    "output": "x/x;x=\"\u00a2\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00a2\";bonus=x",
    "output": "x/x;x=\"\u00a2\";bonus=x"
  },
  {
    "input": "\u00a3/x",
    "output": null
  },
  {
    "input": "x/\u00a3",
    "output": null
  },
  {
    "input": "x/x;\u00a3=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00a3;bonus=x",
    "output": "x/x;x=\"\u00a3\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00a3\";bonus=x",
    "output": "x/x;x=\"\u00a3\";bonus=x"
  },
  {
    "input": "\u00a4/x",
    "output": null
  },
  {
    "input": "x/\u00a4",
    "output": null
  },
  {
    "input": "x/x;\u00a4=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00a4;bonus=x",
    "output": "x/x;x=\"\u00a4\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00a4\";bonus=x",
    "output": "x/x;x=\"\u00a4\";bonus=x"
  },
  {
    "input": "\u00a5/x",
    "output": null
  },
  {
    "input": "x/\u00a5",
    "output": null
  },
  {
    "input": "x/x;\u00a5=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00a5;bonus=x",
    "output": "x/x;x=\"\u00a5\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00a5\";bonus=x",
    "output": "x/x;x=\"\u00a5\";bonus=x"
  },
  {
    "input": "\u00a6/x",
    "output": null
  },
  {
    "input": "x/\u00a6",
    "output": null
  },
  {
    "input": "x/x;\u00a6=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00a6;bonus=x",
    "output": "x/x;x=\"\u00a6\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00a6\";bonus=x",
    "output": "x/x;x=\"\u00a6\";bonus=x"
  },
  {
    "input": "\u00a7/x",
    "output": null
  },
  {
    "input": "x/\u00a7",
    "output": null
  },
  {
    "input": "x/x;\u00a7=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00a7;bonus=x",
    "output": "x/x;x=\"\u00a7\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00a7\";bonus=x",
    "output": "x/x;x=\"\u00a7\";bonus=x"
  },
  {
    "input": "\u00a8/x",
    "output": null
  },
  {
    "input": "x/\u00a8",
    "output": null
  },
  {
    "input": "x/x;\u00a8=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00a8;bonus=x",
    "output": "x/x;x=\"\u00a8\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00a8\";bonus=x",
    "output": "x/x;x=\"\u00a8\";bonus=x"
  },
  {
    "input": "\u00a9/x",
    "output": null
  },
  {
    "input": "x/\u00a9",
    "output": null
  },
  {
    "input": "x/x;\u00a9=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00a9;bonus=x",
    "output": "x/x;x=\"\u00a9\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00a9\";bonus=x",
    "output": "x/x;x=\"\u00a9\";bonus=x"
  },
  {
    "input": "\u00aa/x",
    "output": null
  },
  {
    "input": "x/\u00aa",
    "output": null
  },
  {
    "input": "x/x;\u00aa=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00aa;bonus=x",
    "output": "x/x;x=\"\u00aa\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00aa\";bonus=x",
    "output": "x/x;x=\"\u00aa\";bonus=x"
  },
  {
    "input": "\u00ab/x",
    "output": null
  },
  {
    "input": "x/\u00ab",
    "output": null
  },
  {
    "input": "x/x;\u00ab=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00ab;bonus=x",
    "output": "x/x;x=\"\u00ab\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00ab\";bonus=x",
    "output": "x/x;x=\"\u00ab\";bonus=x"
  },
  {
    "input": "\u00ac/x",
    "output": null
  },
  {
    "input": "x/\u00ac",
    "output": null
  },
  {
    "input": "x/x;\u00ac=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00ac;bonus=x",
    "output": "x/x;x=\"\u00ac\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00ac\";bonus=x",
    "output": "x/x;x=\"\u00ac\";bonus=x"
  },
  {
    "input": "\u00ad/x",
    "output": null
  },
  {
    "input": "x/\u00ad",
    "output": null
  },
  {
    "input": "x/x;\u00ad=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00ad;bonus=x",
    "output": "x/x;x=\"\u00ad\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00ad\";bonus=x",
    "output": "x/x;x=\"\u00ad\";bonus=x"
  },
  {
    "input": "\u00ae/x",
    "output": null
  },
  {
    "input": "x/\u00ae",
    "output": null
  },
  {
    "input": "x/x;\u00ae=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00ae;bonus=x",
    "output": "x/x;x=\"\u00ae\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00ae\";bonus=x",
    "output": "x/x;x=\"\u00ae\";bonus=x"
  },
  {
    "input": "\u00af/x",
    "output": null
  },
  {
    "input": "x/\u00af",
    "output": null
  },
  {
    "input": "x/x;\u00af=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00af;bonus=x",
    "output": "x/x;x=\"\u00af\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00af\";bonus=x",
    "output": "x/x;x=\"\u00af\";bonus=x"
  },
  {
    "input": "\u00b0/x",
    "output": null
  },
  {
    "input": "x/\u00b0",
    "output": null
  },
  {
    "input": "x/x;\u00b0=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00b0;bonus=x",
    "output": "x/x;x=\"\u00b0\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00b0\";bonus=x",
    "output": "x/x;x=\"\u00b0\";bonus=x"
  },
  {
    "input": "\u00b1/x",
    "output": null
  },
  {
    "input": "x/\u00b1",
    "output": null
  },
  {
    "input": "x/x;\u00b1=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00b1;bonus=x",
    "output": "x/x;x=\"\u00b1\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00b1\";bonus=x",
    "output": "x/x;x=\"\u00b1\";bonus=x"
  },
  {
    "input": "\u00b2/x",
    "output": null
  },
  {
    "input": "x/\u00b2",
    "output": null
  },
  {
    "input": "x/x;\u00b2=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00b2;bonus=x",
    "output": "x/x;x=\"\u00b2\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00b2\";bonus=x",
    "output": "x/x;x=\"\u00b2\";bonus=x"
  },
  {
    "input": "\u00b3/x",
    "output": null
  },
  {
    "input": "x/\u00b3",
    "output": null
  },
  {
    "input": "x/x;\u00b3=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00b3;bonus=x",
    "output": "x/x;x=\"\u00b3\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00b3\";bonus=x",
    "output": "x/x;x=\"\u00b3\";bonus=x"
  },
  {
    "input": "\u00b4/x",
    "output": null
  },
  {
    "input": "x/\u00b4",
    "output": null
  },
  {
    "input": "x/x;\u00b4=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00b4;bonus=x",
    "output": "x/x;x=\"\u00b4\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00b4\";bonus=x",
    "output": "x/x;x=\"\u00b4\";bonus=x"
  },
  {
    "input": "\u00b5/x",
    "output": null
  },
  {
    "input": "x/\u00b5",
    "output": null
  },
  {
    "input": "x/x;\u00b5=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00b5;bonus=x",
    "output": "x/x;x=\"\u00b5\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00b5\";bonus=x",
    "output": "x/x;x=\"\u00b5\";bonus=x"
  },
  {
    "input": "\u00b6/x",
    "output": null
  },
  {
    "input": "x/\u00b6",
    "output": null
  },
  {
    "input": "x/x;\u00b6=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00b6;bonus=x",
    "output": "x/x;x=\"\u00b6\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00b6\";bonus=x",
    "output": "x/x;x=\"\u00b6\";bonus=x"
  },
  {
    "input": "\u00b7/x",
    "output": null
  },
  {
    "input": "x/\u00b7",
    "output": null
  },
  {
    "input": "x/x;\u00b7=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00b7;bonus=x",
    "output": "x/x;x=\"\u00b7\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00b7\";bonus=x",
    "output": "x/x;x=\"\u00b7\";bonus=x"
  },
  {
    "input": "\u00b8/x",
    "output": null
  },
  {
    "input": "x/\u00b8",
    "output": null
  },
  {
    "input": "x/x;\u00b8=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00b8;bonus=x",
    "output": "x/x;x=\"\u00b8\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00b8\";bonus=x",
    "output": "x/x;x=\"\u00b8\";bonus=x"
  },
  {
    "input": "\u00b9/x",
    "output": null
  },
  {
    "input": "x/\u00b9",
    "output": null
  },
  {
    "input": "x/x;\u00b9=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00b9;bonus=x",
    "output": "x/x;x=\"\u00b9\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00b9\";bonus=x",
    "output": "x/x;x=\"\u00b9\";bonus=x"
  },
  {
    "input": "\u00ba/x",
    "output": null
  },
  {
    "input": "x/\u00ba",
    "output": null
  },
  {
    "input": "x/x;\u00ba=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00ba;bonus=x",
    "output": "x/x;x=\"\u00ba\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00ba\";bonus=x",
    "output": "x/x;x=\"\u00ba\";bonus=x"
  },
  {
    "input": "\u00bb/x",
    "output": null
  },
  {
    "input": "x/\u00bb",
    "output": null
  },
  {
    "input": "x/x;\u00bb=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00bb;bonus=x",
    "output": "x/x;x=\"\u00bb\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00bb\";bonus=x",
    "output": "x/x;x=\"\u00bb\";bonus=x"
  },
  {
    "input": "\u00bc/x",
    "output": null
  },
  {
    "input": "x/\u00bc",
    "output": null
  },
  {
    "input": "x/x;\u00bc=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00bc;bonus=x",
    "output": "x/x;x=\"\u00bc\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00bc\";bonus=x",
    "output": "x/x;x=\"\u00bc\";bonus=x"
  },
  {
    "input": "\u00bd/x",
    "output": null
  },
  {
    "input": "x/\u00bd",
    "output": null
  },
  {
    "input": "x/x;\u00bd=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00bd;bonus=x",
    "output": "x/x;x=\"\u00bd\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00bd\";bonus=x",
    "output": "x/x;x=\"\u00bd\";bonus=x"
  },
  {
    "input": "\u00be/x",
    "output": null
  },
  {
    "input": "x/\u00be",
    "output": null
  },
  {
    "input": "x/x;\u00be=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00be;bonus=x",
    "output": "x/x;x=\"\u00be\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00be\";bonus=x",
    "output": "x/x;x=\"\u00be\";bonus=x"
  },
  {
    "input": "\u00bf/x",
    "output": null
  },
  {
    "input": "x/\u00bf",
    "output": null
  },
  {
    "input": "x/x;\u00bf=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00bf;bonus=x",
    "output": "x/x;x=\"\u00bf\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00bf\";bonus=x",
    "output": "x/x;x=\"\u00bf\";bonus=x"
  },
  {
    "input": "\u00c0/x",
    "output": null
  },
  {
    "input": "x/\u00c0",
    "output": null
  },
  {
    "input": "x/x;\u00c0=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00c0;bonus=x",
    "output": "x/x;x=\"\u00c0\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00c0\";bonus=x",
    "output": "x/x;x=\"\u00c0\";bonus=x"
  },
  {
    "input": "\u00c1/x",
    "output": null
  },
  {
    "input": "x/\u00c1",
    "output": null
  },
  {
    "input": "x/x;\u00c1=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00c1;bonus=x",
    "output": "x/x;x=\"\u00c1\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00c1\";bonus=x",
    "output": "x/x;x=\"\u00c1\";bonus=x"
  },
  {
    "input": "\u00c2/x",
    "output": null
  },
  {
    "input": "x/\u00c2",
    "output": null
  },
  {
    "input": "x/x;\u00c2=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00c2;bonus=x",
    "output": "x/x;x=\"\u00c2\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00c2\";bonus=x",
    "output": "x/x;x=\"\u00c2\";bonus=x"
  },
  {
    "input": "\u00c3/x",
    "output": null
  },
  {
    "input": "x/\u00c3",
    "output": null
  },
  {
    "input": "x/x;\u00c3=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00c3;bonus=x",
    "output": "x/x;x=\"\u00c3\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00c3\";bonus=x",
    "output": "x/x;x=\"\u00c3\";bonus=x"
  },
  {
    "input": "\u00c4/x",
    "output": null
  },
  {
    "input": "x/\u00c4",
    "output": null
  },
  {
    "input": "x/x;\u00c4=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00c4;bonus=x",
    "output": "x/x;x=\"\u00c4\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00c4\";bonus=x",
    "output": "x/x;x=\"\u00c4\";bonus=x"
  },
  {
    "input": "\u00c5/x",
    "output": null
  },
  {
    "input": "x/\u00c5",
    "output": null
  },
  {
    "input": "x/x;\u00c5=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00c5;bonus=x",
    "output": "x/x;x=\"\u00c5\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00c5\";bonus=x",
    "output": "x/x;x=\"\u00c5\";bonus=x"
  },
  {
    "input": "\u00c6/x",
    "output": null
  },
  {
    "input": "x/\u00c6",
    "output": null
  },
  {
    "input": "x/x;\u00c6=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00c6;bonus=x",
    "output": "x/x;x=\"\u00c6\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00c6\";bonus=x",
    "output": "x/x;x=\"\u00c6\";bonus=x"
  },
  {
    "input": "\u00c7/x",
    "output": null
  },
  {
    "input": "x/\u00c7",
    "output": null
  },
  {
    "input": "x/x;\u00c7=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00c7;bonus=x",
    "output": "x/x;x=\"\u00c7\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00c7\";bonus=x",
    "output": "x/x;x=\"\u00c7\";bonus=x"
  },
  {
    "input": "\u00c8/x",
    "output": null
  },
  {
    "input": "x/\u00c8",
    "output": null
  },
  {
    "input": "x/x;\u00c8=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00c8;bonus=x",
    "output": "x/x;x=\"\u00c8\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00c8\";bonus=x",
    "output": "x/x;x=\"\u00c8\";bonus=x"
  },
  {
    "input": "\u00c9/x",
    "output": null
  },
  {
    "input": "x/\u00c9",
    "output": null
  },
  {
    "input": "x/x;\u00c9=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00c9;bonus=x",
    "output": "x/x;x=\"\u00c9\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00c9\";bonus=x",
    "output": "x/x;x=\"\u00c9\";bonus=x"
  },
  {
    "input": "\u00ca/x",
    "output": null
  },
  {
    "input": "x/\u00ca",
    "output": null
  },
  {
    "input": "x/x;\u00ca=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00ca;bonus=x",
    "output": "x/x;x=\"\u00ca\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00ca\";bonus=x",
    "output": "x/x;x=\"\u00ca\";bonus=x"
  },
  {
    "input": "\u00cb/x",
    "output": null
  },
  {
    "input": "x/\u00cb",
    "output": null
  },
  {
    "input": "x/x;\u00cb=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00cb;bonus=x",
    "output": "x/x;x=\"\u00cb\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00cb\";bonus=x",
    "output": "x/x;x=\"\u00cb\";bonus=x"
  },
  {
    "input": "\u00cc/x",
    "output": null
  },
  {
    "input": "x/\u00cc",
    "output": null
  },
  {
    "input": "x/x;\u00cc=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00cc;bonus=x",
    "output": "x/x;x=\"\u00cc\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00cc\";bonus=x",
    "output": "x/x;x=\"\u00cc\";bonus=x"
  },
  {
    "input": "\u00cd/x",
    "output": null
  },
  {
    "input": "x/\u00cd",
    "output": null
  },
  {
    "input": "x/x;\u00cd=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00cd;bonus=x",
    "output": "x/x;x=\"\u00cd\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00cd\";bonus=x",
    "output": "x/x;x=\"\u00cd\";bonus=x"
  },
  {
    "input": "\u00ce/x",
    "output": null
  },
  {
    "input": "x/\u00ce",
    "output": null
  },
  {
    "input": "x/x;\u00ce=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00ce;bonus=x",
    "output": "x/x;x=\"\u00ce\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00ce\";bonus=x",
    "output": "x/x;x=\"\u00ce\";bonus=x"
  },
  {
    "input": "\u00cf/x",
    "output": null
  },
  {
    "input": "x/\u00cf",
    "output": null
  },
  {
    "input": "x/x;\u00cf=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00cf;bonus=x",
    "output": "x/x;x=\"\u00cf\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00cf\";bonus=x",
    "output": "x/x;x=\"\u00cf\";bonus=x"
  },
  {
    "input": "\u00d0/x",
    "output": null
  },
  {
    "input": "x/\u00d0",
    "output": null
  },
  {
    "input": "x/x;\u00d0=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00d0;bonus=x",
    "output": "x/x;x=\"\u00d0\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00d0\";bonus=x",
    "output": "x/x;x=\"\u00d0\";bonus=x"
  },
  {
    "input": "\u00d1/x",
    "output": null
  },
  {
    "input": "x/\u00d1",
    "output": null
  },
  {
    "input": "x/x;\u00d1=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00d1;bonus=x",
    "output": "x/x;x=\"\u00d1\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00d1\";bonus=x",
    "output": "x/x;x=\"\u00d1\";bonus=x"
  },
  {
    "input": "\u00d2/x",
    "output": null
  },
  {
    "input": "x/\u00d2",
    "output": null
  },
  {
    "input": "x/x;\u00d2=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00d2;bonus=x",
    "output": "x/x;x=\"\u00d2\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00d2\";bonus=x",
    "output": "x/x;x=\"\u00d2\";bonus=x"
  },
  {
    "input": "\u00d3/x",
    "output": null
  },
  {
    "input": "x/\u00d3",
    "output": null
  },
  {
    "input": "x/x;\u00d3=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00d3;bonus=x",
    "output": "x/x;x=\"\u00d3\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00d3\";bonus=x",
    "output": "x/x;x=\"\u00d3\";bonus=x"
  },
  {
    "input": "\u00d4/x",
    "output": null
  },
  {
    "input": "x/\u00d4",
    "output": null
  },
  {
    "input": "x/x;\u00d4=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00d4;bonus=x",
    "output": "x/x;x=\"\u00d4\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00d4\";bonus=x",
    "output": "x/x;x=\"\u00d4\";bonus=x"
  },
  {
    "input": "\u00d5/x",
    "output": null
  },
  {
    "input": "x/\u00d5",
    "output": null
  },
  {
    "input": "x/x;\u00d5=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00d5;bonus=x",
    "output": "x/x;x=\"\u00d5\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00d5\";bonus=x",
    "output": "x/x;x=\"\u00d5\";bonus=x"
  },
  {
    "input": "\u00d6/x",
    "output": null
  },
  {
    "input": "x/\u00d6",
    "output": null
  },
  {
    "input": "x/x;\u00d6=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00d6;bonus=x",
    "output": "x/x;x=\"\u00d6\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00d6\";bonus=x",
    "output": "x/x;x=\"\u00d6\";bonus=x"
  },
  {
    "input": "\u00d7/x",
    "output": null
  },
  {
    "input": "x/\u00d7",
    "output": null
  },
  {
    "input": "x/x;\u00d7=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00d7;bonus=x",
    "output": "x/x;x=\"\u00d7\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00d7\";bonus=x",
    "output": "x/x;x=\"\u00d7\";bonus=x"
  },
  {
    "input": "\u00d8/x",
    "output": null
  },
  {
    "input": "x/\u00d8",
    "output": null
  },
  {
    "input": "x/x;\u00d8=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00d8;bonus=x",
    "output": "x/x;x=\"\u00d8\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00d8\";bonus=x",
    "output": "x/x;x=\"\u00d8\";bonus=x"
  },
  {
    "input": "\u00d9/x",
    "output": null
  },
  {
    "input": "x/\u00d9",
    "output": null
  },
  {
    "input": "x/x;\u00d9=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00d9;bonus=x",
    "output": "x/x;x=\"\u00d9\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00d9\";bonus=x",
    "output": "x/x;x=\"\u00d9\";bonus=x"
  },
  {
    "input": "\u00da/x",
    "output": null
  },
  {
    "input": "x/\u00da",
    "output": null
  },
  {
    "input": "x/x;\u00da=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00da;bonus=x",
    "output": "x/x;x=\"\u00da\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00da\";bonus=x",
    "output": "x/x;x=\"\u00da\";bonus=x"
  },
  {
    "input": "\u00db/x",
    "output": null
  },
  {
    "input": "x/\u00db",
    "output": null
  },
  {
    "input": "x/x;\u00db=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00db;bonus=x",
    "output": "x/x;x=\"\u00db\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00db\";bonus=x",
    "output": "x/x;x=\"\u00db\";bonus=x"
  },
  {
    "input": "\u00dc/x",
    "output": null
  },
  {
    "input": "x/\u00dc",
    "output": null
  },
  {
    "input": "x/x;\u00dc=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00dc;bonus=x",
    "output": "x/x;x=\"\u00dc\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00dc\";bonus=x",
    "output": "x/x;x=\"\u00dc\";bonus=x"
  },
  {
    "input": "\u00dd/x",
    "output": null
  },
  {
    "input": "x/\u00dd",
    "output": null
  },
  {
    "input": "x/x;\u00dd=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00dd;bonus=x",
    "output": "x/x;x=\"\u00dd\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00dd\";bonus=x",
    "output": "x/x;x=\"\u00dd\";bonus=x"
  },
  {
    "input": "\u00de/x",
    "output": null
  },
  {
    "input": "x/\u00de",
    "output": null
  },
  {
    "input": "x/x;\u00de=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00de;bonus=x",
    "output": "x/x;x=\"\u00de\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00de\";bonus=x",
    "output": "x/x;x=\"\u00de\";bonus=x"
  },
  {
    "input": "\u00df/x",
    "output": null
  },
  {
    "input": "x/\u00df",
    "output": null
  },
  {
    "input": "x/x;\u00df=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00df;bonus=x",
    "output": "x/x;x=\"\u00df\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00df\";bonus=x",
    "output": "x/x;x=\"\u00df\";bonus=x"
  },
  {
    "input": "\u00e0/x",
    "output": null
  },
  {
    "input": "x/\u00e0",
    "output": null
  },
  {
    "input": "x/x;\u00e0=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00e0;bonus=x",
    "output": "x/x;x=\"\u00e0\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00e0\";bonus=x",
    "output": "x/x;x=\"\u00e0\";bonus=x"
  },
  {
    "input": "\u00e1/x",
    "output": null
  },
  {
    "input": "x/\u00e1",
    "output": null
  },
  {
    "input": "x/x;\u00e1=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00e1;bonus=x",
    "output": "x/x;x=\"\u00e1\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00e1\";bonus=x",
    "output": "x/x;x=\"\u00e1\";bonus=x"
  },
  {
    "input": "\u00e2/x",
    "output": null
  },
  {
    "input": "x/\u00e2",
    "output": null
  },
  {
    "input": "x/x;\u00e2=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00e2;bonus=x",
    "output": "x/x;x=\"\u00e2\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00e2\";bonus=x",
    "output": "x/x;x=\"\u00e2\";bonus=x"
  },
  {
    "input": "\u00e3/x",
    "output": null
  },
  {
    "input": "x/\u00e3",
    "output": null
  },
  {
    "input": "x/x;\u00e3=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00e3;bonus=x",
    "output": "x/x;x=\"\u00e3\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00e3\";bonus=x",
    "output": "x/x;x=\"\u00e3\";bonus=x"
  },
  {
    "input": "\u00e4/x",
    "output": null
  },
  {
    "input": "x/\u00e4",
    "output": null
  },
  {
    "input": "x/x;\u00e4=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00e4;bonus=x",
    "output": "x/x;x=\"\u00e4\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00e4\";bonus=x",
    "output": "x/x;x=\"\u00e4\";bonus=x"
  },
  {
    "input": "\u00e5/x",
    "output": null
  },
  {
    "input": "x/\u00e5",
    "output": null
  },
  {
    "input": "x/x;\u00e5=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00e5;bonus=x",
    "output": "x/x;x=\"\u00e5\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00e5\";bonus=x",
    "output": "x/x;x=\"\u00e5\";bonus=x"
  },
  {
    "input": "\u00e6/x",
    "output": null
  },
  {
    "input": "x/\u00e6",
    "output": null
  },
  {
    "input": "x/x;\u00e6=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00e6;bonus=x",
    "output": "x/x;x=\"\u00e6\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00e6\";bonus=x",
    "output": "x/x;x=\"\u00e6\";bonus=x"
  },
  {
    "input": "\u00e7/x",
    "output": null
  },
  {
    "input": "x/\u00e7",
    "output": null
  },
  {
    "input": "x/x;\u00e7=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00e7;bonus=x",
    "output": "x/x;x=\"\u00e7\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00e7\";bonus=x",
    "output": "x/x;x=\"\u00e7\";bonus=x"
  },
  {
    "input": "\u00e8/x",
    "output": null
  },
  {
    "input": "x/\u00e8",
    "output": null
  },
  {
    "input": "x/x;\u00e8=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00e8;bonus=x",
    "output": "x/x;x=\"\u00e8\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00e8\";bonus=x",
    "output": "x/x;x=\"\u00e8\";bonus=x"
  },
  {
    "input": "\u00e9/x",
    "output": null
  },
  {
    "input": "x/\u00e9",
    "output": null
  },
  {
    "input": "x/x;\u00e9=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00e9;bonus=x",
    "output": "x/x;x=\"\u00e9\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00e9\";bonus=x",
    "output": "x/x;x=\"\u00e9\";bonus=x"
  },
  {
    "input": "\u00ea/x",
    "output": null
  },
  {
    "input": "x/\u00ea",
    "output": null
  },
  {
    "input": "x/x;\u00ea=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00ea;bonus=x",
    "output": "x/x;x=\"\u00ea\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00ea\";bonus=x",
    "output": "x/x;x=\"\u00ea\";bonus=x"
  },
  {
    "input": "\u00eb/x",
    "output": null
  },
  {
    "input": "x/\u00eb",
    "output": null
  },
  {
    "input": "x/x;\u00eb=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00eb;bonus=x",
    "output": "x/x;x=\"\u00eb\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00eb\";bonus=x",
    "output": "x/x;x=\"\u00eb\";bonus=x"
  },
  {
    "input": "\u00ec/x",
    "output": null
  },
  {
    "input": "x/\u00ec",
    "output": null
  },
  {
    "input": "x/x;\u00ec=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00ec;bonus=x",
    "output": "x/x;x=\"\u00ec\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00ec\";bonus=x",
    "output": "x/x;x=\"\u00ec\";bonus=x"
  },
  {
    "input": "\u00ed/x",
    "output": null
  },
  {
    "input": "x/\u00ed",
    "output": null
  },
  {
    "input": "x/x;\u00ed=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00ed;bonus=x",
    "output": "x/x;x=\"\u00ed\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00ed\";bonus=x",
    "output": "x/x;x=\"\u00ed\";bonus=x"
  },
  {
    "input": "\u00ee/x",
    "output": null
  },
  {
    "input": "x/\u00ee",
    "output": null
  },
  {
    "input": "x/x;\u00ee=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00ee;bonus=x",
    "output": "x/x;x=\"\u00ee\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00ee\";bonus=x",
    "output": "x/x;x=\"\u00ee\";bonus=x"
  },
  {
    "input": "\u00ef/x",
    "output": null
  },
  {
    "input": "x/\u00ef",
    "output": null
  },
  {
    "input": "x/x;\u00ef=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00ef;bonus=x",
    "output": "x/x;x=\"\u00ef\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00ef\";bonus=x",
    "output": "x/x;x=\"\u00ef\";bonus=x"
  },
  {
    "input": "\u00f0/x",
    "output": null
  },
  {
    "input": "x/\u00f0",
    "output": null
  },
  {
    "input": "x/x;\u00f0=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00f0;bonus=x",
    "output": "x/x;x=\"\u00f0\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00f0\";bonus=x",
    "output": "x/x;x=\"\u00f0\";bonus=x"
  },
  {
    "input": "\u00f1/x",
    "output": null
  },
  {
    "input": "x/\u00f1",
    "output": null
  },
  {
    "input": "x/x;\u00f1=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00f1;bonus=x",
    "output": "x/x;x=\"\u00f1\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00f1\";bonus=x",
    "output": "x/x;x=\"\u00f1\";bonus=x"
  },
  {
    "input": "\u00f2/x",
    "output": null
  },
  {
    "input": "x/\u00f2",
    "output": null
  },
  {
    "input": "x/x;\u00f2=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00f2;bonus=x",
    "output": "x/x;x=\"\u00f2\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00f2\";bonus=x",
    "output": "x/x;x=\"\u00f2\";bonus=x"
  },
  {
    "input": "\u00f3/x",
    "output": null
  },
  {
    "input": "x/\u00f3",
    "output": null
  },
  {
    "input": "x/x;\u00f3=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00f3;bonus=x",
    "output": "x/x;x=\"\u00f3\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00f3\";bonus=x",
    "output": "x/x;x=\"\u00f3\";bonus=x"
  },
  {
    "input": "\u00f4/x",
    "output": null
  },
  {
    "input": "x/\u00f4",
    "output": null
  },
  {
    "input": "x/x;\u00f4=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00f4;bonus=x",
    "output": "x/x;x=\"\u00f4\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00f4\";bonus=x",
    "output": "x/x;x=\"\u00f4\";bonus=x"
  },
  {
    "input": "\u00f5/x",
    "output": null
  },
  {
    "input": "x/\u00f5",
    "output": null
  },
  {
    "input": "x/x;\u00f5=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00f5;bonus=x",
    "output": "x/x;x=\"\u00f5\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00f5\";bonus=x",
    "output": "x/x;x=\"\u00f5\";bonus=x"
  },
  {
    "input": "\u00f6/x",
    "output": null
  },
  {
    "input": "x/\u00f6",
    "output": null
  },
  {
    "input": "x/x;\u00f6=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00f6;bonus=x",
    "output": "x/x;x=\"\u00f6\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00f6\";bonus=x",
    "output": "x/x;x=\"\u00f6\";bonus=x"
  },
  {
    "input": "\u00f7/x",
    "output": null
  },
  {
    "input": "x/\u00f7",
    "output": null
  },
  {
    "input": "x/x;\u00f7=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00f7;bonus=x",
    "output": "x/x;x=\"\u00f7\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00f7\";bonus=x",
    "output": "x/x;x=\"\u00f7\";bonus=x"
  },
  {
    "input": "\u00f8/x",
    "output": null
  },
  {
    "input": "x/\u00f8",
    "output": null
  },
  {
    "input": "x/x;\u00f8=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00f8;bonus=x",
    "output": "x/x;x=\"\u00f8\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00f8\";bonus=x",
    "output": "x/x;x=\"\u00f8\";bonus=x"
  },
  {
    "input": "\u00f9/x",
    "output": null
  },
  {
    "input": "x/\u00f9",
    "output": null
  },
  {
    "input": "x/x;\u00f9=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00f9;bonus=x",
    "output": "x/x;x=\"\u00f9\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00f9\";bonus=x",
    "output": "x/x;x=\"\u00f9\";bonus=x"
  },
  {
    "input": "\u00fa/x",
    "output": null
  },
  {
    "input": "x/\u00fa",
    "output": null
  },
  {
    "input": "x/x;\u00fa=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00fa;bonus=x",
    "output": "x/x;x=\"\u00fa\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00fa\";bonus=x",
    "output": "x/x;x=\"\u00fa\";bonus=x"
  },
  {
    "input": "\u00fb/x",
    "output": null
  },
  {
    "input": "x/\u00fb",
    "output": null
  },
  {
    "input": "x/x;\u00fb=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00fb;bonus=x",
    "output": "x/x;x=\"\u00fb\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00fb\";bonus=x",
    "output": "x/x;x=\"\u00fb\";bonus=x"
  },
  {
    "input": "\u00fc/x",
    "output": null
  },
  {
    "input": "x/\u00fc",
    "output": null
  },
  {
    "input": "x/x;\u00fc=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00fc;bonus=x",
    "output": "x/x;x=\"\u00fc\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00fc\";bonus=x",
    "output": "x/x;x=\"\u00fc\";bonus=x"
  },
  {
    "input": "\u00fd/x",
    "output": null
  },
  {
    "input": "x/\u00fd",
    "output": null
  },
  {
    "input": "x/x;\u00fd=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00fd;bonus=x",
    "output": "x/x;x=\"\u00fd\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00fd\";bonus=x",
    "output": "x/x;x=\"\u00fd\";bonus=x"
  },
  {
    "input": "\u00fe/x",
    "output": null
  },
  {
    "input": "x/\u00fe",
    "output": null
  },
  {
    "input": "x/x;\u00fe=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00fe;bonus=x",
    "output": "x/x;x=\"\u00fe\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00fe\";bonus=x",
    "output": "x/x;x=\"\u00fe\";bonus=x"
  },
  {
    "input": "\u00ff/x",
    "output": null
  },
  {
    "input": "x/\u00ff",
    "output": null
  },
  {
    "input": "x/x;\u00ff=x;bonus=x",
    "output": "x/x;bonus=x"
  },
  {
    "input": "x/x;x=\u00ff;bonus=x",
    "output": "x/x;x=\"\u00ff\";bonus=x"
  },
  {
    "input": "x/x;x=\"\u00ff\";bonus=x",
    "output": "x/x;x=\"\u00ff\";bonus=x"
  }
];
