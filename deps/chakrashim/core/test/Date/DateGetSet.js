//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var d = new Date();

// Set some Random dates. 
d.setDate(12345678)
d.setTime(456789);

WScript.Echo("toDateString : " + d.toDateString())
WScript.Echo("getTime : " + d.getTime())
WScript.Echo("getFullYear : " + d.getFullYear())
WScript.Echo("getYear : " + d.getYear())
WScript.Echo("getUTCFullYear : " + d.getUTCFullYear())
WScript.Echo("getMonth : " + d.getMonth())
WScript.Echo("getUTCMonth : " + d.getUTCMonth())
WScript.Echo("getDate : " + d.getDate())
WScript.Echo("getUTCDate : " + d.getUTCDate())
WScript.Echo("getDay : " + d.getDay())
WScript.Echo("getUTCDay : " + d.getUTCDay())
WScript.Echo("getHours : " + d.getHours())
WScript.Echo("getUTCHours : " + d.getUTCHours())
WScript.Echo("getMinutes : " + d.getMinutes())
WScript.Echo("getUTCMinutes : " + d.getUTCMinutes())
WScript.Echo("getSeconds : " + d.getSeconds())
WScript.Echo("getUTCSeconds : " + d.getUTCSeconds())
WScript.Echo("getMilliseconds : " + d.getMilliseconds())
WScript.Echo("getUTCMilliseconds : " + d.getUTCMilliseconds())
WScript.Echo("getTimezoneOffset : " + d.getTimezoneOffset())

// setTime(time)
d.setTime(100);
WScript.Echo("getTime : " + d.getTime());

// setMilliseconds(ms)
d.setMilliseconds(10);
WScript.Echo("getMilliseconds : " + d.getMilliseconds());

// setUTCMilliseconds(ms)
d.setUTCMilliseconds(11);
WScript.Echo("getUTCMilliseconds : " + d.getUTCMilliseconds());

// setSeconds(sec [, ms])
d.setSeconds(12);
WScript.Echo("getSeconds : " + d.getSeconds())

d.setSeconds(13,14);
WScript.Echo("getSeconds : " + d.getSeconds())
WScript.Echo("getMilliseconds : " + d.getMilliseconds());

// setUTCSeconds(sec [, ms])
d.setUTCSeconds(15)
WScript.Echo("getUTCSeconds : " + d.getUTCSeconds())

d.setUTCSeconds(16, 17)
WScript.Echo("getUTCSeconds : " + d.getUTCSeconds())
WScript.Echo("getUTCMilliseconds : " + d.getUTCMilliseconds())

// setMinutes(min [, sec [, ms ] ])
d.setMinutes(18)
WScript.Echo("getMinutes : " + d.getMinutes())

d.setMinutes(19, 20)
WScript.Echo("getMinutes : " + d.getMinutes())
WScript.Echo("getSeconds : " + d.getSeconds())

d.setMinutes(21, 22, 23)
WScript.Echo("getMinutes : " + d.getMinutes())
WScript.Echo("getSeconds : " + d.getSeconds())
WScript.Echo("getMilliseconds : " + d.getMilliseconds());

// setUTCMinutes(min [, sec [, ms ] ])
d.setUTCMinutes(24)
WScript.Echo("getUTCMinutes : " + d.getUTCMinutes())

d.setUTCMinutes(25,26)
WScript.Echo("getUTCMinutes : " + d.getUTCMinutes())
WScript.Echo("getUTCSeconds : " + d.getUTCSeconds())

d.setUTCMinutes(27,28,29)
WScript.Echo("getUTCMinutes : " + d.getUTCMinutes())
WScript.Echo("getUTCSeconds : " + d.getUTCSeconds())
WScript.Echo("getUTCMilliseconds : " + d.getUTCMilliseconds())

// setHours(hour [, min [, sec [, ms ] ] ])
d.setHours(2)
WScript.Echo("getHours : " + d.getHours())

d.setHours(3, 1)
WScript.Echo("getHours : " + d.getHours())
WScript.Echo("getMinutes : " + d.getMinutes())

d.setHours(4, 2, 3)
WScript.Echo("getHours : " + d.getHours())
WScript.Echo("getMinutes : " + d.getMinutes())
WScript.Echo("getSeconds : " + d.getSeconds())

d.setHours(5, 6, 7, 8)
WScript.Echo("getHours : " + d.getHours())
WScript.Echo("getMinutes : " + d.getMinutes())
WScript.Echo("getSeconds : " + d.getSeconds())
WScript.Echo("getMilliseconds : " + d.getMilliseconds());

// setUTCHours(hour [, min [, sec [, ms ] ] ])
d.setUTCHours(2)
WScript.Echo("getUTCHours : " + d.getUTCHours())

d.setUTCHours(3, 1)
WScript.Echo("getUTCHours : " + d.getUTCHours())
WScript.Echo("getUTCMinutes : " + d.getUTCMinutes())

d.setUTCHours(4, 2, 3)
WScript.Echo("getUTCHours : " + d.getUTCHours())
WScript.Echo("getUTCMinutes : " + d.getUTCMinutes())
WScript.Echo("getUTCSeconds : " + d.getUTCSeconds())

d.setUTCHours(5, 6, 7, 8)
WScript.Echo("getUTCHours : " + d.getUTCHours())
WScript.Echo("getUTCMinutes : " + d.getUTCMinutes())
WScript.Echo("getUTCSeconds : " + d.getUTCSeconds())
WScript.Echo("getUTCMilliseconds : " + d.getUTCMilliseconds());

// setDate(date)
d.setDate(1000);
WScript.Echo("getDate : " + d.getDate())

// setUTCDate(date)
d.setUTCDate(2000)
WScript.Echo("getUTCDate : " + d.getUTCDate())

// setMonth(month [, date ])
d.setMonth(7)
WScript.Echo("getMonth : " + d.getMonth())

d.setMonth(8, 3000)
WScript.Echo("getMonth : " + d.getMonth())
WScript.Echo("getDate : " + d.getDate())

// setUTCMonth(month [, date])
d.setUTCMonth(7)
WScript.Echo("getUTCMonth : " + d.getUTCMonth())

d.setUTCMonth(8, 3000)
WScript.Echo("getUTCMonth : " + d.getUTCMonth())
WScript.Echo("getUTCDate : " + d.getUTCDate())

// setFullYear(year [, month [, date ] ])
d.setFullYear(2009)
WScript.Echo("getFullYear : " + d.getFullYear())

// setYear(year [, month [, date ] ])
d.setYear(2009)
WScript.Echo("getYear : " + d.getYear())

d.setFullYear(2010, 10)
WScript.Echo("getFullYear : " + d.getFullYear())
WScript.Echo("getMonth : " + d.getMonth())

d.setFullYear(2011, 11, 1234)
WScript.Echo("getFullYear : " + d.getFullYear())
WScript.Echo("getMonth : " + d.getMonth())
WScript.Echo("getDate : " + d.getDate())

// setUTCFullYear(year [, month [, date ] ])
d.setUTCFullYear(2009)
WScript.Echo("getUTCFullYear : " + d.getUTCFullYear())

d.setUTCFullYear(2010, 10)
WScript.Echo("getUTCFullYear : " + d.getUTCFullYear())
WScript.Echo("getUTCMonth : " + d.getUTCMonth())

d.setUTCFullYear(2011, 11, 1234)
WScript.Echo("getUTCFullYear : " + d.getUTCFullYear())
WScript.Echo("getUTCMonth : " + d.getUTCMonth())
WScript.Echo("getUTCDate : " + d.getUTCDate())

d.setDate(12345678);
WScript.Echo("toUTCString : " + d.toUTCString())
WScript.Echo("valueOf : " + d.valueOf())

WScript.Echo("toISOString method : " + typeof d.toISOString);
WScript.Echo("toJSON method : " + typeof d.toJSON);

// Set fullYear/fullYear+month/year on the Date prototype
Date.prototype.setYear(5);                  // Year
WScript.Echo(Date.prototype.getFullYear());
Date.prototype.setYear(4, 4);               // Year, month -- month should be ignored
WScript.Echo(Date.prototype.getFullYear());
WScript.Echo(Date.prototype.getMonth());
Date.prototype.setFullYear(1999);           // Only full year
WScript.Echo(Date.prototype.getFullYear());
Date.prototype.setFullYear(1998, 5);        // Full year and month
WScript.Echo(Date.prototype.getFullYear());
WScript.Echo(Date.prototype.getMonth());
