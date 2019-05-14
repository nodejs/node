// These are defined by the test:
// errors (boolean)
// encoder (function)
// ranges (array)
// separator (string)
// expect (function)

var tests = [];
var cplist = [];
var numTests = null;
var numFrames = 2;
var chunkSize = 400;
var numChunks = null;
var frames = null;
var frames = null;
var forms = null;
var encodedSeparator = encodeURIComponent(separator);
var currentChunkIndex = 0;
var pageCharset = document.querySelector("meta[charset]").getAttribute("charset");

setup(function() {
    // create a simple list of just those code points for which there is an encoding possible
    codepoints = [];
    for (var range of ranges) {
        for (var i = range[0]; i < range[1]; i++) {
            result = encoder(String.fromCodePoint(i));
            var success = !!result;
            if (errors) {
              success = !success;
            }
            if (success) {
                var item = {};
                codepoints.push(item);
                item.cp = i;
                item.expected = expect(result, i);
                item.desc = range[2];
            }
        }
    }

    // convert the information into a simple array of objects that can be easily traversed
    var currentChunk = [];
    var currentTests = [];
    cplist = [currentChunk];
    tests = [currentTests];
    for (i = 0; i < codepoints.length; i++) {
        if (currentChunk.length == chunkSize) {
            currentChunk = [];
            cplist.push(currentChunk);
            currentTests = [];
            tests.push(currentTests);
        }
        var item = {};
        currentChunk.push(item);
        item.cp = codepoints[i].cp;
        item.expected = codepoints[i].expected;
        item.desc = codepoints[i].desc;
        currentTests.push(subsetTest(async_test,
                                     (item.desc ? item.desc + " " : "") +
                                     "U+" +
                                     item.cp.toString(16).toUpperCase() +
                                     " " +
                                     String.fromCodePoint(item.cp) +
                                     " " +
                                     item.expected
        ));
    }

    numChunks = cplist.length;

    for (var i = 0; i < numFrames; i++) {
        var frame = document.createElement("iframe");
        frame.id = frame.name = "frame-" + i;
        document.body.appendChild(frame);
        var form = document.createElement("form");
        form.id = "form-" + i;
        form.method = "GET";
        form.action = "/common/blank.html";
        form.acceptCharset = pageCharset;
        form.target = frame.id;
        var input = document.createElement("input");
        input.id = input.name = "input-" + i;
        form.appendChild(input);
        document.body.appendChild(form);
    }

    addEventListener("load", function() {
        frames = Array.prototype.slice.call(
            document.getElementsByTagName("iframe")
        );
        forms = Array.prototype.slice.call(
            document.getElementsByTagName("form")
        );
        inputs = Array.prototype.slice.call(
            document.getElementsByTagName("input")
        );
        for (var i = 0; i < Math.min(numFrames, numChunks); i++) {
            runNext(i);
        }
    });
});

function runNext(id) {
    var i = currentChunkIndex;
    currentChunkIndex += 1;

    var iframe = frames[id];
    var form = forms[id];
    var input = inputs[id];

    input.value = cplist[i]
        .map(function(x) {
            return String.fromCodePoint(x.cp);
        })
        .join(separator);
    form.submit();

    iframe.onload = function() {
        var url = iframe.contentWindow.location;
        var query = url.search;
        var result_string = query.substr(query.indexOf("=") + 1);
        var results = result_string.split(encodedSeparator);

        for (var j = 0; j < cplist[i].length; j++) {
            var t = tests[i][j];
            if (t) {
                t.step(function() {
                    assert_equals(
                        normalizeStr(results[j]),
                        normalizeStr(cplist[i][j].expected)
                    );
                });
                t.done();
            }
        }
        if (currentChunkIndex < numChunks) {
            runNext(id);
        }
    };
}
