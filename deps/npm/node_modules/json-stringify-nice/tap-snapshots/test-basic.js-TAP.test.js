/* IMPORTANT
 * This snapshot file is auto-generated, but designed for humans.
 * It should be checked into source control and tracked carefully.
 * Re-generate by setting TAP_SNAPSHOT=1 and running tests.
 * Make sure to inspect the output below.  Do not ignore changes!
 */
'use strict'
exports[`test/basic.js TAP basic sorting operation with default 2-space indent > mix of objects and out of order keys 1`] = `
{
  "y": "z",
  "yy": "a",
  "z": 1,
  "a": {
    "a": 2,
    "b": 1
  },
  "obj": {
    "b": "x",
    "a": {}
  }
}

`

exports[`test/basic.js TAP replacer function is used > replace a val with phone doggo 1`] = `
{
  "y": "z",
  "yy": "a",
  "z": 1,
  "a": {
    "b": 1,
    "hello": "📞 yes",
    "this is": "🐕",
    "a": {
      "hello": "📞 yes",
      "nested": true,
      "this is": "🐕"
    }
  },
  "obj": {
    "b": "x",
    "a": {
      "hello": "📞 yes",
      "this is": "🐕"
    }
  }
}

`

exports[`test/basic.js TAP sort keys explicitly with a preference list > replace a val with preferences 1`] = `
{
  "z": 1,
  "yy": "a",
  "y": "z",
  "obj": {
    "b": "x",
    "a": {}
  },
  "a": {
    "b": 1,
    "a": {
      "nested": true
    }
  }
}

`

exports[`test/basic.js TAP spaces can be set > boolean false 1`] = `
{"y":"z","yy":"a","z":1,"a":{"a":2,"b":1},"obj":{"b":"x","a":{}}}
`

exports[`test/basic.js TAP spaces can be set > empty string 1`] = `
{"y":"z","yy":"a","z":1,"a":{"a":2,"b":1},"obj":{"b":"x","a":{}}}
`

exports[`test/basic.js TAP spaces can be set > space face 1`] = `
{
 ^_^ "y": "z",
 ^_^ "yy": "a",
 ^_^ "z": 1,
 ^_^ "a": {
 ^_^  ^_^ "a": 2,
 ^_^  ^_^ "b": 1
 ^_^ },
 ^_^ "obj": {
 ^_^  ^_^ "b": "x",
 ^_^  ^_^ "a": {}
 ^_^ }
}

`

exports[`test/basic.js TAP spaces can be set > tab 1`] = `
{
	"y": "z",
	"yy": "a",
	"z": 1,
	"a": {
		"a": 2,
		"b": 1
	},
	"obj": {
		"b": "x",
		"a": {}
	}
}

`

exports[`test/basic.js TAP spaces can be set > the number 3 1`] = `
{
   "y": "z",
   "yy": "a",
   "z": 1,
   "a": {
      "a": 2,
      "b": 1
   },
   "obj": {
      "b": "x",
      "a": {}
   }
}

`
