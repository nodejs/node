var tests = [];

function iframeRef(frameRef) {
    return frameRef.contentWindow
        ? frameRef.contentWindow.document
        : frameRef.contentDocument;
}

function showNodes(decoder) {
    var iframe = iframeRef(document.getElementById("scrwin"));
    nodes = iframe.querySelectorAll("span");

    for (var i = 0; i < nodes.length; i++) {
        var test = subsetTest(async_test,
                              "U+" +
                              nodes[i].dataset.cp +
                              " " +
                              String.fromCodePoint(parseInt(nodes[i].dataset.cp, 16)) +
                              " " +
                              decoder(nodes[i].dataset.bytes) +
                              " " +
                              nodes[i].dataset.bytes
        );
        if (test) {
            tests[i] = test;
        }
    }

    for (var i = 0; i < nodes.length; i++) {
        if (tests[i]) {
            tests[i].step(function() {
                assert_equals(
                    nodes[i].textContent,
                    decoder(nodes[i].dataset.bytes)
                );
            });
            tests[i].done();
        }
    }
}
