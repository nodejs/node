//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// String tag function tests -- verifies behavior of functions like String.prototype.link

if (this.WScript && this.WScript.LoadScriptFile) { // Check for running in ch
    this.WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");
}

var tests = [
    {
        name: "Quote characters are escaped correctly",
        body: function() {
            var _this = 'value';
            var property = 'any string with a quote " longer than 32 characters';
            
            assert.areEqual('<a href="any string with a quote &quot; longer than 32 characters">value</a>', _this.link(property), "Verify String.prototype.link escapes quote characters correctly");
            
            _this = '""""""""""""""""';
            property = '""""""""""""""""';
            
            assert.areEqual('<a name="&quot;&quot;&quot;&quot;&quot;&quot;&quot;&quot;&quot;&quot;&quot;&quot;&quot;&quot;&quot;&quot;">""""""""""""""""</a>', _this.anchor(property), "Multiple quote characters");
            
            _this = 'value';
            property = 'long string with " multiple " quote characters " and " the final character is a quote "';
            
            assert.areEqual('<font size="long string with &quot; multiple &quot; quote characters &quot; and &quot; the final character is a quote &quot;">value</font>', _this.fontsize(property), "The final character in the property is a quote");
        }
    },
    {
        name: "String without null terminator at length",
        body: function() {
            var _this = 'value';
            var property = "-\"";
            
            assert.areEqual('<a href="-">value</a>', _this.link(property.substring(1,0)), "String.prototype.substring reuses the string buffer but returns a string with a different length");
        }
    },
    {
        name: "String with embedded null characters",
        body: function() {
            var _this = 'value';
            var property = " a string with quotes \"\" and an embedded null \0 character";
            var result = _this.fontcolor(property);
            
            assert.areEqual("<font color=\" a string with quotes &quot;&quot; and an embedded null \0 character\">value</font>", result, "Embedded null character doesn't truncate the property we copy (string will be truncated in output)");
            assert.areEqual(94, result.length, "Length of string is correct even if it's truncated in test output");
            
            result = "\0".fontsize("\0");
            
            assert.areEqual("<font size=\"\0\">\0</font>", result, "Embedded null characters in both property and this field");
            assert.areEqual(23, result.length, "Length of string is correct even though it may show up truncated in the test output");
        }
    },
    {
        name: "Zero-length strings",
        body: function() {
            var _this = 'value';
            var property = '';
            
            assert.areEqual("<font size=\"\">value</font>", _this.fontsize(property), "Zero-length property string doesn't cause null dereference");
            
            _this = '';
            
            assert.areEqual("<a href=\"\"></a>", _this.link(property), "Zero-length property and this strings");
        }
    },
    {
        name: "Function with no property",
        body: function() {
            var _this = 'value';
            
            assert.areEqual("<blink>value</blink>", _this.blink(), "String tag functions with no property work correctly");
        }
    }
];

testRunner.runTests(tests);
