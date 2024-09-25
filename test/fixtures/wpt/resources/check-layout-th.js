(function() {
// Test is initiated from body.onload, so explicit done() call is required.
setup({ explicit_done: true });

function checkSubtreeExpectedValues(t, parent, prefix)
{
    var checkedLayout = checkExpectedValues(t, parent, prefix);
    Array.prototype.forEach.call(parent.childNodes, function(node) {
        checkedLayout |= checkSubtreeExpectedValues(t, node, prefix);
    });
    return checkedLayout;
}

function checkAttribute(output, node, attribute)
{
    var result = node.getAttribute && node.getAttribute(attribute);
    output.checked |= !!result;
    return result;
}

function assert_tolerance(actual, expected, message)
{
    if (isNaN(expected) || isNaN(actual) || Math.abs(actual - expected) >= 1) {
        assert_equals(actual, Number(expected), message);
    }
}

function checkDataKeys(node) {
  // The purpose of this list of data-* attributes is simply to ensure typos
  // in tests are caught. It is therefore "ok" to add to this list for
  // specific tests.
    var validData = new Set([
        "data-anchor-polyfill",
        "data-expected-width",
        "data-expected-height",
        "data-offset-x",
        "data-offset-y",
        "data-expected-client-width",
        "data-expected-client-height",
        "data-expected-scroll-width",
        "data-expected-scroll-height",
        "data-expected-bounding-client-rect-width",
        "data-expected-bounding-client-rect-height",
        "data-total-x",
        "data-total-y",
        "data-expected-display",
        "data-expected-padding-top",
        "data-expected-padding-bottom",
        "data-expected-padding-left",
        "data-expected-padding-right",
        "data-expected-margin-top",
        "data-expected-margin-bottom",
        "data-expected-margin-left",
        "data-expected-margin-right"
    ]);
    if (!node || !node.getAttributeNames)
        return;
    // Use "data-test" prefix if you need custom-named data elements.
    for (let name of node.getAttributeNames()) {
        if (name.startsWith("data-") && !name.startsWith("data-test"))
            assert_true(validData.has(name), name + " is a valid data attribute");
    }
}

function checkExpectedValues(t, node, prefix)
{
    checkDataKeys(node);
    var output = { checked: false };

    var expectedWidth = checkAttribute(output, node, "data-expected-width");
    if (expectedWidth) {
        assert_tolerance(node.offsetWidth, expectedWidth, prefix + "width");
    }

    var expectedHeight = checkAttribute(output, node, "data-expected-height");
    if (expectedHeight) {
        assert_tolerance(node.offsetHeight, expectedHeight, prefix + "height");
    }

    var expectedOffset = checkAttribute(output, node, "data-offset-x");
    if (expectedOffset) {
        assert_tolerance(node.offsetLeft, expectedOffset, prefix + "offsetLeft");
    }

    var expectedOffset = checkAttribute(output, node, "data-offset-y");
    if (expectedOffset) {
        assert_tolerance(node.offsetTop, expectedOffset, prefix + "offsetTop");
    }

    var expectedWidth = checkAttribute(output, node, "data-expected-client-width");
    if (expectedWidth) {
        assert_tolerance(node.clientWidth, expectedWidth, prefix + "clientWidth");
    }

    var expectedHeight = checkAttribute(output, node, "data-expected-client-height");
    if (expectedHeight) {
        assert_tolerance(node.clientHeight, expectedHeight, prefix + "clientHeight");
    }

    var expectedWidth = checkAttribute(output, node, "data-expected-scroll-width");
    if (expectedWidth) {
        assert_tolerance(node.scrollWidth, expectedWidth, prefix + "scrollWidth");
    }

    var expectedHeight = checkAttribute(output, node, "data-expected-scroll-height");
    if (expectedHeight) {
        assert_tolerance(node.scrollHeight, expectedHeight, prefix + "scrollHeight");
    }

    var expectedWidth = checkAttribute(output, node, "data-expected-bounding-client-rect-width");
    if (expectedWidth) {
        assert_tolerance(node.getBoundingClientRect().width, expectedWidth, prefix + "getBoundingClientRect().width");
    }

    var expectedHeight = checkAttribute(output, node, "data-expected-bounding-client-rect-height");
    if (expectedHeight) {
        assert_tolerance(node.getBoundingClientRect().height, expectedHeight, prefix + "getBoundingClientRect().height");
    }

    var expectedOffset = checkAttribute(output, node, "data-total-x");
    if (expectedOffset) {
        var totalLeft = node.clientLeft + node.offsetLeft;
        assert_tolerance(totalLeft, expectedOffset, prefix +
                         "clientLeft+offsetLeft (" + node.clientLeft + " + " + node.offsetLeft + ")");
    }

    var expectedOffset = checkAttribute(output, node, "data-total-y");
    if (expectedOffset) {
        var totalTop = node.clientTop + node.offsetTop;
        assert_tolerance(totalTop, expectedOffset, prefix +
                         "clientTop+offsetTop (" + node.clientTop + " + " + node.offsetTop + ")");
    }

    var expectedDisplay = checkAttribute(output, node, "data-expected-display");
    if (expectedDisplay) {
        var actualDisplay = getComputedStyle(node).display;
        assert_equals(actualDisplay, expectedDisplay, prefix + "display");
    }

    var expectedPaddingTop = checkAttribute(output, node, "data-expected-padding-top");
    if (expectedPaddingTop) {
        var actualPaddingTop = getComputedStyle(node).paddingTop;
        // Trim the unit "px" from the output.
        actualPaddingTop = actualPaddingTop.slice(0, -2);
        assert_equals(actualPaddingTop, expectedPaddingTop, prefix + "padding-top");
    }

    var expectedPaddingBottom = checkAttribute(output, node, "data-expected-padding-bottom");
    if (expectedPaddingBottom) {
        var actualPaddingBottom = getComputedStyle(node).paddingBottom;
        // Trim the unit "px" from the output.
        actualPaddingBottom = actualPaddingBottom.slice(0, -2);
        assert_equals(actualPaddingBottom, expectedPaddingBottom, prefix + "padding-bottom");
    }

    var expectedPaddingLeft = checkAttribute(output, node, "data-expected-padding-left");
    if (expectedPaddingLeft) {
        var actualPaddingLeft = getComputedStyle(node).paddingLeft;
        // Trim the unit "px" from the output.
        actualPaddingLeft = actualPaddingLeft.slice(0, -2);
        assert_equals(actualPaddingLeft, expectedPaddingLeft, prefix + "padding-left");
    }

    var expectedPaddingRight = checkAttribute(output, node, "data-expected-padding-right");
    if (expectedPaddingRight) {
        var actualPaddingRight = getComputedStyle(node).paddingRight;
        // Trim the unit "px" from the output.
        actualPaddingRight = actualPaddingRight.slice(0, -2);
        assert_equals(actualPaddingRight, expectedPaddingRight, prefix + "padding-right");
    }

    var expectedMarginTop = checkAttribute(output, node, "data-expected-margin-top");
    if (expectedMarginTop) {
        var actualMarginTop = getComputedStyle(node).marginTop;
        // Trim the unit "px" from the output.
        actualMarginTop = actualMarginTop.slice(0, -2);
        assert_equals(actualMarginTop, expectedMarginTop, prefix + "margin-top");
    }

    var expectedMarginBottom = checkAttribute(output, node, "data-expected-margin-bottom");
    if (expectedMarginBottom) {
        var actualMarginBottom = getComputedStyle(node).marginBottom;
        // Trim the unit "px" from the output.
        actualMarginBottom = actualMarginBottom.slice(0, -2);
        assert_equals(actualMarginBottom, expectedMarginBottom, prefix + "margin-bottom");
    }

    var expectedMarginLeft = checkAttribute(output, node, "data-expected-margin-left");
    if (expectedMarginLeft) {
        var actualMarginLeft = getComputedStyle(node).marginLeft;
        // Trim the unit "px" from the output.
        actualMarginLeft = actualMarginLeft.slice(0, -2);
        assert_equals(actualMarginLeft, expectedMarginLeft, prefix + "margin-left");
    }

    var expectedMarginRight = checkAttribute(output, node, "data-expected-margin-right");
    if (expectedMarginRight) {
        var actualMarginRight = getComputedStyle(node).marginRight;
        // Trim the unit "px" from the output.
        actualMarginRight = actualMarginRight.slice(0, -2);
        assert_equals(actualMarginRight, expectedMarginRight, prefix + "margin-right");
    }

    return output.checked;
}

var testNumber = 0;
var highlightError = false; // displays outline around failed test element.
var printDomOnError = true; // prints dom when test fails.

window.checkLayout = function(selectorList, callDone = true)
{
    if (!selectorList) {
        console.error("You must provide a CSS selector of nodes to check.");
        return;
    }
    var nodes = document.querySelectorAll(selectorList);
    nodes = Array.prototype.slice.call(nodes);
    var checkedLayout = false;
    Array.prototype.forEach.call(nodes, function(node) {
        const title = node.title == '' ? '' : `: ${node.title}`;
        test(function(t) {
            var container = node.parentNode.className == 'container' ? node.parentNode : node;
            var prefix =
                printDomOnError ? '\n' + container.outerHTML + '\n' : '';
            var passed = false;
            try {
                checkedLayout |= checkExpectedValues(t, node.parentNode, prefix);
                checkedLayout |= checkSubtreeExpectedValues(t, node, prefix);
                passed = true;
            } finally {
              if (!passed && highlightError) {
                if (!document.getElementById('testharness_error_css')) {
                  var style = document.createElement('style');
                  style.id = 'testharness_error_css';
                  style.textContent = '.testharness_error { outline: red dotted 2px !important; }';
                  document.body.appendChild(style);
                }
                if (node)
                  node.classList.add('testharness_error');
              }
                checkedLayout |= !passed;
            }
        }, `${selectorList} ${++testNumber}${title}`);
    });
    if (!checkedLayout) {
        console.error("No valid data-* attributes found in selector list : " + selectorList);
    }
    if (callDone)
        done();
};

})();
