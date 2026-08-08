"use strict";

// Tests that most of the functionality of the window.performance object is available in web workers.

importScripts("/resources/testharness.js");

function verifyEntry (entry, name, type, duration, assertName) {
    assert_equals(entry.name, name, assertName + " has the right name");
    assert_equals(entry.entryType, type, assertName + " has the right type");
    assert_equals(entry.duration, duration, assertName + "has the right duration");
}

var start;
test(function testPerformanceNow () {
    start = performance.now();
}, "Can use performance.now in workers");

test(function testPerformanceMark () {
    while (performance.now() == start) { }
    performance.mark("mark1");
     // Stall the minimum amount of time to ensure the marks are separate
    var now = performance.now();
    while (performance.now() == now) { }
    performance.mark("mark2");
}, "Can use performance.mark in workers");

test(function testPerformanceMeasure () {
    performance.measure("measure1", "mark1", "mark2");
}, "Can use performance.measure in workers");

test(function testPerformanceGetEntriesByName () {
    var mark1s = performance.getEntriesByName("mark1");
    assert_equals(mark1s.length, 1, "getEntriesByName gave correct number of entries");
    verifyEntry(mark1s[0], "mark1", "mark", 0, "Entry got by name");
}, "Can use performance.getEntriesByName in workers");

var marks;
var measures;
test(function testPerformanceGetEntriesByType () {
    marks = performance.getEntriesByType("mark");
    assert_equals(marks.length, 2, "getEntriesByType gave correct number of entries");
    verifyEntry(marks[0], "mark1", "mark", 0, "First mark entry");
    verifyEntry(marks[1], "mark2", "mark", 0, "Second mark entry");
    measures = performance.getEntriesByType("measure");
    assert_equals(measures.length, 1, "getEntriesByType(\"measure\") gave correct number of entries");
    verifyEntry(measures[0], "measure1", "measure", marks[1].startTime - marks[0].startTime, "Measure entry");
}, "Can use performance.getEntriesByType in workers");

test(function testPerformanceEntryOrder () {
    assert_greater_than(marks[0].startTime, start, "First mark startTime is after a time before it");
    assert_greater_than(marks[1].startTime, marks[0].startTime, "Second mark startTime is after first mark startTime");
    assert_equals(measures[0].startTime, marks[0].startTime, "measure's startTime is the first mark's startTime");
}, "Performance marks and measures seem to be working correctly in workers");

test(function testPerformanceClearing () {
    performance.clearMarks();
    assert_equals(performance.getEntriesByType("mark").length, 0, "clearMarks cleared the marks");
    performance.clearMeasures();
    assert_equals(performance.getEntriesByType("measure").length, 0, "clearMeasures cleared the measures");
}, "Can use clearMarks and clearMeasures in workers");

test(function testPerformanceResourceTiming () {  // Resource timing
    var start = performance.now();
    var xhr = new XMLHttpRequest();
     // Do a synchronous request and add a little artificial delay
    xhr.open("GET", "/resources/testharness.js?pipe=trickle(d0.25)", false);
    xhr.send();
     // The browser might or might not have added a resource performance entry for the importScripts() above; we're only interested in xmlhttprequest entries
    var entries = performance.getEntriesByType("resource").filter(entry => entry.initiatorType == "xmlhttprequest");
    assert_equals(entries.length, 1, "getEntriesByType(\"resource\") returns one entry with initiatorType of xmlhttprequest");
    assert_true(!!entries[0].name.match(/\/resources\/testharness.js/), "Resource entry has loaded url as its name");
    assert_equals(entries[0].entryType, "resource", "Resource entry has correct entryType");
    assert_equals(entries[0].initiatorType, "xmlhttprequest", "Resource entry has correct initiatorType");
    var currentTimestamp = start;
    var currentTimestampName = "a time before it";
    [
        "startTime", "fetchStart", "requestStart", "responseStart", "responseEnd"
    ].forEach((name) => {
        var timestamp = entries[0][name];
        // We want to skip over values that are 0 because of TAO securty rescritions
        // Or else this test will fail. This can happen for "requestStart", "responseStart".
        if (timestamp != 0) {
            assert_greater_than_equal(timestamp, currentTimestamp, "Resource entry " + name + " is after " + currentTimestampName);
            currentTimestamp = timestamp;
            currentTimestampName = name;
        }
    });
    assert_greater_than(entries[0].responseEnd, entries[0].startTime, "The resource request should have taken at least some time");
     // We requested a delay of 250ms, but it could be a little bit less, and could be significantly more.
    assert_greater_than(entries[0].responseEnd - entries[0].responseStart, 230, "Resource timing numbers reflect reality somewhat");
}, "Resource timing seems to work in workers");

test(function testPerformanceClearResourceTimings () {
    performance.clearResourceTimings();
    assert_equals(performance.getEntriesByType("resource").length, 0, "clearResourceTimings cleared the resource timings");
}, "performance.clearResourceTimings in workers");

test(function testPerformanceSetResourceTimingBufferSize () {
    performance.setResourceTimingBufferSize(0);
    var xhr = new XMLHttpRequest();
    xhr.open("GET", "/resources/testharness.js", false);  // synchronous request
    xhr.send();
    assert_equals(performance.getEntriesByType("resource").length, 0, "setResourceTimingBufferSize(0) prevents resource entries from being added");
}, "performance.setResourceTimingBufferSize in workers");

test(function testPerformanceHasNoTiming () {
    assert_equals(typeof(performance.timing), "undefined", "performance.timing is undefined");
}, "performance.timing is not available in workers");

test(function testPerformanceHasNoNavigation () {
    assert_equals(typeof(performance.navigation), "undefined", "performance.navigation is undefined");
}, "performance.navigation is not available in workers");

test(function testPerformanceHasToJSON () {
    assert_equals(typeof(performance.toJSON), "function", "performance.toJSON is a function");
}, "performance.toJSON is available in workers");

test(function testPerformanceNoNavigationEntries () {
    assert_equals(performance.getEntriesByType("navigation").length, 0, "getEntriesByType(\"navigation\") returns nothing");
    assert_equals(performance.getEntriesByName("document", "navigation").length, 0, "getEntriesByName(\"document\", \"navigation\") returns nothing");
    assert_equals(performance.getEntriesByName("document").length, 0, "getEntriesByName(\"document\") returns nothing");
    var hasNavigation = performance.getEntries().some((e,i,a) => {
        return e.entryType == "navigation";
    });
    assert_false(hasNavigation, "getEntries should return no navigation entries.");

}, "There are no navigation type performance entries in workers");

done();
