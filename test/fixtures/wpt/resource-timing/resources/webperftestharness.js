/*
author: W3C http://www.w3.org/
help: http://www.w3.org/TR/navigation-timing/#sec-window.performance-attribute
*/
//
// Helper Functions for ResourceTiming W3C tests
//

var performanceNamespace = window.performance;
var timingAttributes = [
    'connectEnd',
    'connectStart',
    'domComplete',
    'domContentLoadedEventEnd',
    'domContentLoadedEventStart',
    'domInteractive',
    'domLoading',
    'domainLookupEnd',
    'domainLookupStart',
    'fetchStart',
    'loadEventEnd',
    'loadEventStart',
    'navigationStart',
    'redirectEnd',
    'redirectStart',
    'requestStart',
    'responseEnd',
    'responseStart',
    'unloadEventEnd',
    'unloadEventStart'
];

var namespace_check = false;

//
// All test() functions in the WebPerf test suite should use wp_test() instead.
//
// wp_test() validates the window.performance namespace exists prior to running tests and
// immediately shows a single failure if it does not.
//

function wp_test(func, msg, properties)
{
    // only run the namespace check once
    if (!namespace_check)
    {
        namespace_check = true;

        if (performanceNamespace === undefined || performanceNamespace == null)
        {
            // show a single error that window.performance is undefined
            // The window.performance attribute provides a hosting area for performance related attributes.
            test(function() { assert_true(performanceNamespace !== undefined && performanceNamespace != null, "window.performance is defined and not null"); }, "window.performance is defined and not null.");
        }
    }

    test(func, msg, properties);
}

function test_namespace(child_name, skip_root)
{
    if (skip_root === undefined) {
        var msg = 'window.performance is defined';
        // The window.performance attribute provides a hosting area for performance related attributes.
        wp_test(function () { assert_not_equals(performanceNamespace, undefined, msg); }, msg);
    }

    if (child_name !== undefined) {
        var msg2 = 'window.performance.' + child_name + ' is defined';
        // The window.performance attribute provides a hosting area for performance related attributes.
        wp_test(function() { assert_not_equals(performanceNamespace[child_name], undefined, msg2); }, msg2);
    }
}

function test_attribute_exists(parent_name, attribute_name, properties)
{
    var msg = 'window.performance.' + parent_name + '.' + attribute_name + ' is defined.';
    wp_test(function() { assert_not_equals(performanceNamespace[parent_name][attribute_name], undefined, msg); }, msg, properties);
}

function test_enum(parent_name, enum_name, value, properties)
{
    var msg = 'window.performance.' + parent_name + '.' + enum_name + ' is defined.';
    wp_test(function() { assert_not_equals(performanceNamespace[parent_name][enum_name], undefined, msg); }, msg, properties);

    msg = 'window.performance.' + parent_name + '.' + enum_name + ' = ' + value;
    wp_test(function() { assert_equals(performanceNamespace[parent_name][enum_name], value, msg); }, msg, properties);
}

function test_timing_order(attribute_name, greater_than_attribute, properties)
{
    // ensure it's not 0 first
    var msg = "window.performance.timing." + attribute_name + " > 0";
    wp_test(function() { assert_true(performanceNamespace.timing[attribute_name] > 0, msg); }, msg, properties);

    // ensure it's in the right order
    msg = "window.performance.timing." + attribute_name + " >= window.performance.timing." + greater_than_attribute;
    wp_test(function() { assert_true(performanceNamespace.timing[attribute_name] >= performanceNamespace.timing[greater_than_attribute], msg); }, msg, properties);
}

function test_timing_greater_than(attribute_name, greater_than, properties)
{
    var msg = "window.performance.timing." + attribute_name + " > " + greater_than;
    test_greater_than(performanceNamespace.timing[attribute_name], greater_than, msg, properties);
}

function test_timing_equals(attribute_name, equals, msg, properties)
{
    var test_msg = msg || "window.performance.timing." + attribute_name + " == " + equals;
    test_equals(performanceNamespace.timing[attribute_name], equals, test_msg, properties);
}

//
// Non-test related helper functions
//

function sleep_milliseconds(n)
{
    var start = new Date().getTime();
    while (true) {
        if ((new Date().getTime() - start) >= n) break;
    }
}

//
// Common helper functions
//

function test_true(value, msg, properties)
{
    wp_test(function () { assert_true(value, msg); }, msg, properties);
}

function test_equals(value, equals, msg, properties)
{
    wp_test(function () { assert_equals(value, equals, msg); }, msg, properties);
}

function test_greater_than(value, greater_than, msg, properties)
{
    wp_test(function () { assert_true(value > greater_than, msg); }, msg, properties);
}

function test_greater_or_equals(value, greater_than, msg, properties)
{
    wp_test(function () { assert_true(value >= greater_than, msg); }, msg, properties);
}

function test_not_equals(value, notequals, msg, properties)
{
    wp_test(function() { assert_not_equals(value, notequals, msg); }, msg, properties);
}

function test_tao_pass(entry) {
    test_greater_than(entry.redirectStart, 0, 'redirectStart > 0 in cross-origin redirect with Timing-Allow-Origin.');
    test_greater_than(entry.redirectEnd, 0, 'redirectEnd > 0 in cross-origin redirect with Timing-Allow-Origin.');
    test_greater_than(entry.fetchStart, 0, 'fetchStart > 0 in cross-origin redirect with Timing-Allow-Origin.');
    test_greater_than(entry.fetchStart, entry.startTime, 'startTime < fetchStart in cross-origin redirect with Timing-Allow-Origin.');
}

function test_tao_fail(entry) {
    test_equals(entry.redirectStart, 0, 'redirectStart == 0 in cross-origin redirect with failing Timing-Allow-Origin.');
    test_equals(entry.redirectEnd, 0, 'redirectEnd == 0 in cross-origin redirect with failing Timing-Allow-Origin.');
    test_greater_than(entry.fetchStart, 0, 'fetchStart > 0 in cross-origin redirect with failing Timing-Allow-Origin.');
    test_equals(entry.fetchStart, entry.startTime, 'startTime == fetchStart in cross-origin redirect with failing Timing-Allow-Origin.');
}
