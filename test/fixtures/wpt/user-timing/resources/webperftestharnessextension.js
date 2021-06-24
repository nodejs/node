//
// Helper functions for User Timing tests
//

var mark_names = [
    '',
    '1',
    'abc',
];

var measures = [
    [''],
    ['2', 1],
    ['aaa', 'navigationStart', ''],
];

function test_method_exists(method, method_name, properties)
{
    var msg;
    if (typeof method === 'function')
        msg = 'performance.' + method.name + ' is supported!';
    else
        msg = 'performance.' + method_name + ' is supported!';
    wp_test(function() { assert_equals(typeof method, 'function', msg); }, msg, properties);
}

function test_method_throw_exception(func_str, exception, msg)
{
    let exception_name;
    let test_func;
    if (typeof exception == "function") {
        exception_name = exception.name;
        test_func = assert_throws_js;
    } else {
        exception_name = exception;
        test_func = assert_throws_dom;
    }
    var msg = 'Invocation of ' + func_str + ' should throw ' + exception_name  + ' Exception.';
    wp_test(function() { test_func(exception, function() {eval(func_str)}, msg); }, msg);
}

function test_noless_than(value, greater_than, msg, properties)
{
    wp_test(function () { assert_true(value >= greater_than, msg); }, msg, properties);
}

function test_fail(msg, properties)
{
    wp_test(function() { assert_unreached(); }, msg, properties);
}

function test_resource_entries(entries, expected_entries)
{
    // This is slightly convoluted so that we can sort the output.
    var actual_entries = {};
    var origin = window.location.protocol + "//" + window.location.host;

    for (var i = 0; i < entries.length; ++i) {
        var entry = entries[i];
        var found = false;
        for (var expected_entry in expected_entries) {
            if (entry.name == origin + expected_entry) {
                found = true;
                if (expected_entry in actual_entries) {
                    test_fail(expected_entry + ' is not expected to have duplicate entries');
                }
                actual_entries[expected_entry] = entry;
                break;
            }
        }
        if (!found) {
            test_fail(entries[i].name + ' is not expected to be in the Resource Timing buffer');
        }
    }

    sorted_urls = [];
    for (var i in actual_entries) {
        sorted_urls.push(i);
    }
    sorted_urls.sort();
    for (var i in sorted_urls) {
        var url = sorted_urls[i];
        test_equals(actual_entries[url].initiatorType,
                    expected_entries[url],
                    origin + url + ' is expected to have initiatorType ' + expected_entries[url]);
    }
    for (var j in expected_entries) {
        if (!(j in actual_entries)) {
            test_fail(origin + j + ' is expected to be in the Resource Timing buffer');
        }
    }
}

function performance_entrylist_checker(type)
{
    const entryType = type;

    function entry_check(entry, expectedNames, testDescription = '')
    {
        const msg = testDescription + 'Entry \"' + entry.name + '\" should be one that we have set.';
        wp_test(function() { assert_in_array(entry.name, expectedNames, msg); }, msg);
        test_equals(entry.entryType, entryType, testDescription + 'entryType should be \"' + entryType + '\".');
        if (type === "measure") {
            test_true(isFinite(entry.startTime), testDescription + 'startTime should be a number.');
            test_true(isFinite(entry.duration), testDescription + 'duration should be a number.');
        } else if (type === "mark") {
            test_greater_than(entry.startTime, 0, testDescription + 'startTime should greater than 0.');
            test_equals(entry.duration, 0, testDescription + 'duration of mark should be 0.');
        }
    }

    function entrylist_order_check(entryList)
    {
        let inOrder = true;
        for (let i = 0; i < entryList.length - 1; ++i)
        {
            if (entryList[i + 1].startTime < entryList[i].startTime) {
                inOrder = false;
                break;
            }
        }
        return inOrder;
    }

    function entrylist_check(entryList, expectedLength, expectedNames, testDescription = '')
    {
        test_equals(entryList.length, expectedLength, testDescription + 'There should be ' + expectedLength + ' entries.');
        test_true(entrylist_order_check(entryList), testDescription + 'Entries in entrylist should be in order.');
        for (let i = 0; i < entryList.length; ++i)
        {
            entry_check(entryList[i], expectedNames, testDescription + 'Entry_list ' + i + '. ');
        }
    }

    return{"entrylist_check":entrylist_check};
}

function PerformanceContext(context)
{
    this.performanceContext = context;
}

PerformanceContext.prototype =
{

    initialMeasures: function(item, index, array)
    {
        this.performanceContext.measure.apply(this.performanceContext, item);
    },

    mark: function()
    {
        this.performanceContext.mark.apply(this.performanceContext, arguments);
    },

    measure: function()
    {
        this.performanceContext.measure.apply(this.performanceContext, arguments);
    },

    clearMarks: function()
    {
        this.performanceContext.clearMarks.apply(this.performanceContext, arguments);
    },

    clearMeasures: function()
    {
        this.performanceContext.clearMeasures.apply(this.performanceContext, arguments);

    },

    getEntries: function()
    {
        return this.performanceContext.getEntries.apply(this.performanceContext, arguments);
    },

    getEntriesByType: function()
    {
        return this.performanceContext.getEntriesByType.apply(this.performanceContext, arguments);
    },

    getEntriesByName: function()
    {
        return this.performanceContext.getEntriesByName.apply(this.performanceContext, arguments);
    },

    setResourceTimingBufferSize: function()
    {
        return this.performanceContext.setResourceTimingBufferSize.apply(this.performanceContext, arguments);
    },

    registerResourceTimingBufferFullCallback: function(func)
    {
        this.performanceContext.onresourcetimingbufferfull = func;
    },

    clearResourceTimings: function()
    {
        this.performanceContext.clearResourceTimings.apply(this.performanceContext, arguments);
    }

};
