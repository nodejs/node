"use strict";

window.onload =
    function () {
        setup({ explicit_timeout: true });

        /** Number of milliseconds to delay when the server injects pauses into the response.

            This should be large enough that we can distinguish it from noise with high confidence,
            but small enough that tests complete quickly. */
        var serverStepDelay = 250;

        var mimeHtml    = "text/html";
        var mimeText    = "text/plain";
        var mimePng     = "image/png";
        var mimeScript  = "application/javascript";
        var mimeCss     = "text/css";

        /** Hex encoding of a a 150x50px green PNG. */
        var greenPng = "0x89504E470D0A1A0A0000000D494844520000006400000032010300000090FBECFD00000003504C544500FF00345EC0A80000000F49444154281563601805A36068020002BC00011BDDE3900000000049454E44AE426082";

        /** Array containing test cases to run.  Initially, it contains the one-off 'about:blank" test,
            but additional cases are pushed below by expanding templates. */
        var testCases = [
            {
                description: "No timeline entry for about:blank",
                test:
                    function (test) {
                        // Insert an empty IFrame.
                        var frame = document.createElement("iframe");

                        // Wait for the IFrame to load and ensure there is no resource entry for it on the timeline.
                        //
                        // We use the 'createOnloadCallbackFn()' helper which is normally invoked by 'initiateFetch()'
                        // to avoid setting the IFrame's src.  It registers a test step for us, finds our entry on the
                        // resource timeline, and wraps our callback function to automatically vet invariants.
                        frame.onload = createOnloadCallbackFn(test, frame, "about:blank",
                            function (initiator, entry) {
                                assert_equals(entry, undefined, "Inserting an IFrame with a src of 'about:blank' must not add an entry to the timeline.");
                                assertInvariants(
                                    test,
                                    function () {
                                        test.done();
                                    });
                            });

                        document.body.appendChild(frame);

                        // Paranoid check that the new IFrame has loaded about:blank.
                        assert_equals(
                            frame.contentWindow.location.href,
                            "about:blank",
                            "'Src' of new <iframe> must be 'about:blank'.");
                    }
            },
        ];

        // Create cached/uncached tests from the following array of templates.  For each template entry,
        // we add two identical test cases to 'testCases'.  The first case initiates a fetch to populate the
        // cache.  The second request initiates a fetch with the same URL to cover the case where we hit
        // the cache (if the caching policy permits caching).
        [
            { initiator: "iframe",         response: "(done)",      mime: mimeHtml },
            { initiator: "xmlhttprequest", response: "(done)",      mime: mimeText },
            // Multiple browsers seem to cheat a bit and race onLoad of images.  Microsoft https://developer.microsoft.com/en-us/microsoft-edge/platform/issues/2379187
            // { initiator: "img",            response: greenPng,      mime: mimePng },
            { initiator: "script",         response: '"";',         mime: mimeScript },
            { initiator: "link",           response: ".unused{}",   mime: mimeCss },
        ]
        .forEach(function (template) {
            testCases.push({
                description: "'" + template.initiator + " (Populate cache): The initial request populates the cache (if appropriate).",
                test: function (test) {
                    initiateFetch(
                        test,
                        template.initiator,
                        getSyntheticUrl(
                            "mime:" + encodeURIComponent(template.mime)
                                + "&send:" + encodeURIComponent(template.response),
                            /* allowCaching = */ true),
                        function (initiator, entry) {
                            test.done();
                        });
                    }
                });

            testCases.push({
                description: "'" + template.initiator + " (Potentially Cached): Immediately fetch the same URL, exercising the cache hit path (if any).",
                test: function (test) {
                    initiateFetch(
                        test,
                        template.initiator,
                        getSyntheticUrl(
                            "mime:" + encodeURIComponent(template.mime)
                                + "&send:" + encodeURIComponent(template.response),
                            /* allowCaching = */ true),
                        function (initiator, entry) {
                            test.done();
                        });
                    }
                });
            });

        // Create responseStart/responseEnd tests from the following array of templates.  In this test, the server delays before
        // responding with responsePart1, then delays again before completing with responsePart2.  The test looks for the expected
        // pauses before responseStart and responseEnd.
        [
            { initiator: "iframe",         responsePart1: serverStepDelay + "ms;", responsePart2: (serverStepDelay * 2) + "ms;(done)",                                                      mime: mimeHtml },
            { initiator: "xmlhttprequest", responsePart1: serverStepDelay + "ms;", responsePart2: (serverStepDelay * 2) + "ms;(done)",                                                      mime: mimeText },
            // Multiple browsers seem to cheat a bit and race img.onLoad and setting responseEnd.  Microsoft https://developer.microsoft.com/en-us/microsoft-edge/platform/issues/2379187
            // { initiator: "img",            responsePart1: greenPng.substring(0, greenPng.length / 2), responsePart2: "0x" + greenPng.substring(greenPng.length / 2, greenPng.length),       mime: mimePng },
            { initiator: "script",         responsePart1: '"', responsePart2: '";',                                                                                                         mime: mimeScript },
            { initiator: "link",           responsePart1: ".unused{", responsePart2: "}",                                                                                                   mime: mimeCss },
        ]
        .forEach(function (template) {
            testCases.push({
                description: "'" + template.initiator + ": " + serverStepDelay + "ms delay before 'responseStart', another " + serverStepDelay + "ms delay before 'responseEnd'.",
                test: function (test) {
                    initiateFetch(
                        test,
                        template.initiator,
                        getSyntheticUrl(serverStepDelay + "ms"                          // Wait, then echo back responsePart1
                            + "&mime:" + encodeURIComponent(template.mime)
                            + "&send:" + encodeURIComponent(template.responsePart1)
                            + "&" + serverStepDelay + "ms"                              // Wait, then echo back responsePart2
                            + "&send:" + encodeURIComponent(template.responsePart2)),

                        function (initiator, entry) {
                            // Per https://w3c.github.io/resource-timing/#performanceresourcetiming:
                            // If no redirects (or equivalent) occur, this redirectStart/End must return zero.
                            assert_equals(entry.redirectStart, 0, "When no redirect occurs, redirectStart must be 0.");
                            assert_equals(entry.redirectEnd, 0, "When no redirect occurs, redirectEnd must be 0.");

                            // Server creates a gap between 'requestStart' and 'responseStart'.
                            assert_greater_than_equal(
                                entry.responseStart,
                                entry.requestStart + serverStepDelay,
                                "'responseStart' must be " + serverStepDelay + "ms later than 'requestStart'.");

                            // Server creates a gap between 'responseStart' and 'responseEnd'.
                            assert_greater_than_equal(
                                entry.responseEnd,
                                entry.responseStart + serverStepDelay,
                                "'responseEnd' must be " + serverStepDelay + "ms later than 'responseStart'.");

                            test.done();
                        });
                    }
                });
            });

        // Create redirectEnd/responseStart tests from the following array of templates.  In this test, the server delays before
        // redirecting to a new synthetic response, then delays again before responding with 'response'.  The test looks for the
        // expected pauses before redirectEnd and responseStart.
        [
            { initiator: "iframe",         response: serverStepDelay + "ms;redirect;" + (serverStepDelay * 2) + "ms;(done)",        mime: mimeHtml },
            { initiator: "xmlhttprequest", response: serverStepDelay + "ms;redirect;" + (serverStepDelay * 2) + "ms;(done)",        mime: mimeText },
            // Multiple browsers seem to cheat a bit and race img.onLoad and setting responseEnd.  Microsoft https://developer.microsoft.com/en-us/microsoft-edge/platform/issues/2379187
            // { initiator: "img",            response: greenPng,                                                                      mime: mimePng },
            { initiator: "script",         response: '"";',                                                                         mime: mimeScript },
            { initiator: "link",           response: ".unused{}",                                                                   mime: mimeCss },
        ]
        .forEach(function (template) {
            testCases.push({
                description: "'" + template.initiator + " (Redirected): " + serverStepDelay + "ms delay before 'redirectEnd', another " + serverStepDelay + "ms delay before 'responseStart'.",
                test: function (test) {
                    initiateFetch(
                        test,
                        template.initiator,
                        getSyntheticUrl(serverStepDelay + "ms"      // Wait, then redirect to a second page that waits
                            + "&redirect:"                          // before echoing back the response.
                                + encodeURIComponent(
                                    getSyntheticUrl(serverStepDelay + "ms"
                                        + "&mime:" + encodeURIComponent(template.mime)
                                        + "&send:" + encodeURIComponent(template.response)))),
                        function (initiator, entry) {
                            // Per https://w3c.github.io/resource-timing/#performanceresourcetiming:
                            //      "[If redirected, startTime] MUST return the same value as redirectStart.
                            assert_equals(entry.startTime, entry.redirectStart, "startTime must be equal to redirectStart.");

                            // Server creates a gap between 'redirectStart' and 'redirectEnd'.
                            assert_greater_than_equal(
                                entry.redirectEnd,
                                entry.redirectStart + serverStepDelay,
                                "'redirectEnd' must be " + serverStepDelay + "ms later than 'redirectStart'.");

                            // Server creates a gap between 'requestStart' and 'responseStart'.
                            assert_greater_than_equal(
                                entry.responseStart,
                                entry.requestStart + serverStepDelay,
                                "'responseStart' must be " + serverStepDelay + "ms later than 'requestStart'.");

                            test.done();
                        });
                    }
                });
            });

        // Ensure that responseStart only measures the time up to the first few
        // bytes of the header response. This is tested by writing an HTTP 1.1
        // status line, followed by a flush, then a pause before the end of the
        // headers. The test makes sure that responseStart is not delayed by
        // this pause.
        [
            { initiator: "iframe",         response: "(done)",    mime: mimeHtml },
            { initiator: "xmlhttprequest", response: "(done)",    mime: mimeText },
            { initiator: "script",         response: '"";',       mime: mimeScript },
            { initiator: "link",           response: ".unused{}", mime: mimeCss },
        ]
        .forEach(function (template) {
            testCases.push({
                description: "'" + template.initiator + " " + serverStepDelay + "ms delay in headers does not affect responseStart'",
                test: function (test) {
                    initiateFetch(
                        test,
                        template.initiator,
                        getSyntheticUrl("status:200"
                                        + "&flush"
                                        + "&" + serverStepDelay + "ms"
                                        + "&mime:" + template.mime
                                        + "&send:" + encodeURIComponent(template.response)),
                        function (initiator, entry) {
                            // Test that the delay between 'responseStart' and
                            // 'responseEnd' includes the delay, which implies
                            // that 'responseStart' was measured at the time of
                            // status line receipt.
                            assert_greater_than_equal(
                                entry.responseEnd,
                                entry.responseStart + serverStepDelay,
                                "Delay after HTTP/1.1 status should not affect 'responseStart'.");

                            test.done();
                        });
                    }
                });
            });

        // Function to run the next case in the queue.
        var currentTestIndex = -1;
        function runNextCase() {
            var testCase = testCases[++currentTestIndex];
            if (testCase !== undefined) {
                async_test(testCase.test, testCase.description);
            }
        }

        // When a test completes, run the next case in the queue.
        add_result_callback(runNextCase);

        // Start the first test.
        runNextCase();

        /** Iterates through all resource entries on the timeline, vetting all invariants. */
        function assertInvariants(test, done) {
            // Multiple browsers seem to cheat a bit and race img.onLoad and setting responseEnd.  Microsoft https://developer.microsoft.com/en-us/microsoft-edge/platform/issues/2379187
            // Yield for 100ms to workaround a suspected race where window.onload fires before
            //     script visible side-effects from the wininet/urlmon thread have finished.
            test.step_timeout(
                test.step_func(
                    function () {
                        performance
                            .getEntriesByType("resource")
                            .forEach(
                                function (entry, index, entries) {
                                    assertResourceEntryInvariants(entry);
                                });

                        done();
                    }),
                    100);
        }

        /** Assets the invariants for a resource timeline entry. */
        function assertResourceEntryInvariants(actual) {
            // Example from http://w3c.github.io/resource-timing/#resources-included:
            //     "If an HTML IFRAME element is added via markup without specifying a src attribute,
            //      the user agent may load the about:blank document for the IFRAME. If at a later time
            //      the src attribute is changed dynamically via script, the user agent may fetch the new
            //      URL resource for the IFRAME. In this case, only the fetch of the new URL would be
            //      included as a PerformanceResourceTiming object in the Performance Timeline."
            assert_not_equals(
                actual.name,
                "about:blank",
                "Fetch for 'about:blank' must not appear in timeline.");

            assert_not_equals(actual.startTime, 0, "startTime");

            // Per https://w3c.github.io/resource-timing/#performanceresourcetiming:
            //      "[If redirected, startTime] MUST return the same value as redirectStart. Otherwise,
            //      [startTime] MUST return the same value as fetchStart."
            assert_in_array(actual.startTime, [actual.redirectStart, actual.fetchStart],
                "startTime must be equal to redirectStart or fetchStart.");

            // redirectStart <= redirectEnd <= fetchStart <= domainLookupStart <= domainLookupEnd <= connectStart
            assert_less_than_equal(actual.redirectStart, actual.redirectEnd, "redirectStart <= redirectEnd");
            assert_less_than_equal(actual.redirectEnd, actual.fetchStart, "redirectEnd <= fetchStart");
            assert_less_than_equal(actual.fetchStart, actual.domainLookupStart, "fetchStart <= domainLookupStart");
            assert_less_than_equal(actual.domainLookupStart, actual.domainLookupEnd, "domainLookupStart <= domainLookupEnd");
            assert_less_than_equal(actual.domainLookupEnd, actual.connectStart, "domainLookupEnd <= connectStart");

            // Per https://w3c.github.io/resource-timing/#performanceresourcetiming:
            //      "This attribute is optional. User agents that don't have this attribute available MUST set it
            //      as undefined.  [...]  If the secureConnectionStart attribute is available but HTTPS is not used,
            //      this attribute MUST return zero."
            assert_true(actual.secureConnectionStart == undefined ||
                        actual.secureConnectionStart == 0 ||
                        actual.secureConnectionStart >= actual.connectEnd, "secureConnectionStart time");

            // connectStart <= connectEnd <= requestStart <= responseStart <= responseEnd
            assert_less_than_equal(actual.connectStart, actual.connectEnd, "connectStart <= connectEnd");
            assert_less_than_equal(actual.connectEnd, actual.requestStart, "connectEnd <= requestStart");
            assert_less_than_equal(actual.requestStart, actual.responseStart, "requestStart <= responseStart");
            assert_less_than_equal(actual.responseStart, actual.responseEnd, "responseStart <= responseEnd");
        }

        /** Helper function to resolve a relative URL */
        function canonicalize(url) {
            var div = document.createElement('div');
            div.innerHTML = "<a></a>";
            div.firstChild.href = url;
            div.innerHTML = div.innerHTML;
            return div.firstChild.href;
        }

        /** Generates a unique string, used by getSyntheticUrl() to avoid hitting the cache. */
        function createUniqueQueryArgument() {
            var result =
                "ignored_"
                    + Date.now()
                    + "-"
                    + ((Math.random() * 0xFFFFFFFF) >>> 0)
                    + "-"
                    + syntheticRequestCount;

            return result;
        }

        /** Count of the calls to getSyntheticUrl().  Used by createUniqueQueryArgument() to generate unique strings. */
        var syntheticRequestCount = 0;

        /** Return a URL to a server that will synthesize an HTTP response using the given
            commands. (See SyntheticResponse.aspx). */
        function getSyntheticUrl(commands, allowCache) {
            syntheticRequestCount++;

            var url =
                canonicalize("./SyntheticResponse.py")    // ASP.NET page that will synthesize the response.
                    + "?" + commands;                       // Commands that will be used.

            if (allowCache !== true) {                      // If caching is disallowed, append a unique argument
                url += "&" + createUniqueQueryArgument();   // to the URL's query string.
            }

            return url;
        }

        /** Given an 'initiatorType' (e.g., "img") , it triggers the appropriate type of fetch for the specified
            url and invokes 'onloadCallback' when the fetch completes.  If the fetch caused an entry to be created
            on the resource timeline, the entry is passed to the callback. */
        function initiateFetch(test, initiatorType, url, onloadCallback) {
            assertInvariants(
                test,
                function () {
                    log("--- Begin: " + url);

                    switch (initiatorType) {
                        case "script":
                        case "img":
                        case "iframe": {
                            var element = document.createElement(initiatorType);
                            document.body.appendChild(element);
                            element.onload = createOnloadCallbackFn(test, element, url, onloadCallback);
                            element.src = url;
                            break;
                        }
                        case "link": {
                            var element = document.createElement(initiatorType);
                            element.rel = "stylesheet";
                            document.body.appendChild(element);
                            element.onload = createOnloadCallbackFn(test, element, url, onloadCallback);
                            element.href = url;
                            break;
                        }
                        case "xmlhttprequest": {
                            var xhr = new XMLHttpRequest();
                            xhr.open('GET', url, true);
                            xhr.onreadystatechange = createOnloadCallbackFn(test, xhr, url, onloadCallback);
                            xhr.send();
                            break;
                        }
                        default:
                            assert_unreached("Unsupported initiatorType '" + initiatorType + "'.");
                            break;
                    }});
        }

        /** Used by 'initiateFetch' to register a test step for the asynchronous callback, vet invariants,
            find the matching resource timeline entry (if any), and pass it to the given 'onloadCallback'
            when invoked. */
        function createOnloadCallbackFn(test, initiator, url, onloadCallback) {
            // Remember the number of entries on the timeline prior to initiating the fetch:
            var beforeEntryCount = performance.getEntriesByType("resource").length;

            return test.step_func(
                function() {
                    // If the fetch was initiated by XHR, we're subscribed to the 'onreadystatechange' event.
                    // Ignore intermediate callbacks and wait for the XHR to complete.
                    if (Object.getPrototypeOf(initiator) === XMLHttpRequest.prototype) {
                        if (initiator.readyState != 4) {
                            return;
                        }
                    }

                    var entries = performance.getEntriesByType("resource");
                    var candidateEntry = entries[entries.length - 1];

                    switch (entries.length - beforeEntryCount)
                    {
                        case 0:
                            candidateEntry = undefined;
                            break;
                        case 1:
                            // Per https://w3c.github.io/resource-timing/#performanceresourcetiming:
                            //     "This attribute MUST return the resolved URL of the requested resource. This attribute
                            //      MUST NOT change even if the fetch redirected to a different URL."
                            assert_equals(candidateEntry.name, url, "'name' did not match expected 'url'.");
                            logResourceEntry(candidateEntry);
                            break;
                        default:
                            assert_unreached("At most, 1 entry should be added to the performance timeline during a fetch.");
                            break;
                    }

                    assertInvariants(
                        test,
                        function () {
                            onloadCallback(initiator, candidateEntry);
                        });
                });
        }

        /** Log the given text to the document element with id='output' */
        function log(text) {
            var output = document.getElementById("output");
            output.textContent += text + "\r\n";
        }

        add_completion_callback(function () {
            var output = document.getElementById("output");
            var button = document.createElement('button');
            output.parentNode.insertBefore(button, output);
            button.onclick = function () {
                var showButton = output.style.display == 'none';
                output.style.display = showButton ? null : 'none';
                button.textContent = showButton ? 'Hide details' : 'Show details';
            }
            button.onclick();
            var iframes = document.querySelectorAll('iframe');
            for (var i = 0; i < iframes.length; i++)
                iframes[i].parentNode.removeChild(iframes[i]);
        });

        /** pretty print a resource timeline entry. */
        function logResourceEntry(entry) {
            log("[" + entry.entryType + "] " + entry.name);

            ["startTime", "redirectStart", "redirectEnd", "fetchStart", "domainLookupStart", "domainLookupEnd", "connectStart", "secureConnectionStart", "connectEnd", "requestStart", "responseStart", "responseEnd"]
                .forEach(
                    function (property, index, array) {
                        var value = entry[property];
                        log(property + ":\t" + value);
                    });

            log("\r\n");
        }
    };
