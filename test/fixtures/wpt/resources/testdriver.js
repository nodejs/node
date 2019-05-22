(function() {
    "use strict";
    var idCounter = 0;

    function getInViewCenterPoint(rect) {
        var left = Math.max(0, rect.left);
        var right = Math.min(window.innerWidth, rect.right);
        var top = Math.max(0, rect.top);
        var bottom = Math.min(window.innerHeight, rect.bottom);

        var x = 0.5 * (left + right);
        var y = 0.5 * (top + bottom);

        return [x, y];
    }

    function getPointerInteractablePaintTree(element) {
        if (!window.document.contains(element)) {
            return [];
        }

        var rectangles = element.getClientRects();

        if (rectangles.length === 0) {
            return [];
        }

        var centerPoint = getInViewCenterPoint(rectangles[0]);

        if ("elementsFromPoint" in document) {
            return document.elementsFromPoint(centerPoint[0], centerPoint[1]);
        } else if ("msElementsFromPoint" in document) {
            var rv = document.msElementsFromPoint(centerPoint[0], centerPoint[1]);
            return Array.prototype.slice.call(rv ? rv : []);
        } else {
            throw new Error("document.elementsFromPoint unsupported");
        }
    }

    function inView(element) {
        var pointerInteractablePaintTree = getPointerInteractablePaintTree(element);
        return pointerInteractablePaintTree.indexOf(element) !== -1;
    }


    /**
     * @namespace
     */
    window.test_driver = {
        /**
         * Trigger user interaction in order to grant additional privileges to
         * a provided function.
         *
         * https://html.spec.whatwg.org/#triggered-by-user-activation
         *
         * @param {String} intent - a description of the action which much be
         *                          triggered by user interaction
         * @param {Function} action - code requiring escalated privileges
         *
         * @returns {Promise} fulfilled following user interaction and
         *                    execution of the provided `action` function;
         *                    rejected if interaction fails or the provided
         *                    function throws an error
         */
        bless: function(intent, action) {
            var button = document.createElement("button");
            button.innerHTML = "This test requires user interaction.<br />" +
                "Please click here to allow " + intent + ".";
            button.id = "wpt-test-driver-bless-" + (idCounter += 1);
            const elem = document.body || document.documentElement;
            elem.appendChild(button);

            return new Promise(function(resolve, reject) {
                    button.addEventListener("click", resolve);

                    test_driver.click(button).catch(reject);
                }).then(function() {
                    button.remove();

                    if (typeof action === "function") {
                        return action();
                    }
                });
        },

        /**
         * Triggers a user-initiated click
         *
         * This matches the behaviour of the {@link
         * https://w3c.github.io/webdriver/webdriver-spec.html#element-click|WebDriver
         * Element Click command}.
         *
         * @param {Element} element - element to be clicked
         * @returns {Promise} fulfilled after click occurs, or rejected in
         *                    the cases the WebDriver command errors
         */
        click: function(element) {
            if (window.top !== window) {
                return Promise.reject(new Error("can only click in top-level window"));
            }

            if (!window.document.contains(element)) {
                return Promise.reject(new Error("element in different document or shadow tree"));
            }

            if (!inView(element)) {
                element.scrollIntoView({behavior: "instant",
                                        block: "end",
                                        inline: "nearest"});
            }

            var pointerInteractablePaintTree = getPointerInteractablePaintTree(element);
            if (pointerInteractablePaintTree.length === 0 ||
                !element.contains(pointerInteractablePaintTree[0])) {
                return Promise.reject(new Error("element click intercepted error"));
            }

            var rect = element.getClientRects()[0];
            var centerPoint = getInViewCenterPoint(rect);
            return window.test_driver_internal.click(element,
                                                     {x: centerPoint[0],
                                                      y: centerPoint[1]});
        },

        /**
         * Send keys to an element
         *
         * This matches the behaviour of the {@link
         * https://w3c.github.io/webdriver/webdriver-spec.html#element-send-keys|WebDriver
         * Send Keys command}.
         *
         * @param {Element} element - element to send keys to
         * @param {String} keys - keys to send to the element
         * @returns {Promise} fulfilled after keys are sent, or rejected in
         *                    the cases the WebDriver command errors
         */
        send_keys: function(element, keys) {
            if (window.top !== window) {
                return Promise.reject(new Error("can only send keys in top-level window"));
            }

            if (!window.document.contains(element)) {
                return Promise.reject(new Error("element in different document or shadow tree"));
            }

            if (!inView(element)) {
                element.scrollIntoView({behavior: "instant",
                                        block: "end",
                                        inline: "nearest"});
            }

            var pointerInteractablePaintTree = getPointerInteractablePaintTree(element);
            if (pointerInteractablePaintTree.length === 0 ||
                !element.contains(pointerInteractablePaintTree[0])) {
                return Promise.reject(new Error("element send_keys intercepted error"));
            }

            return window.test_driver_internal.send_keys(element, keys);
        },

        /**
         * Freeze the current page
         *
         * The freeze function transitions the page from the HIDDEN state to
         * the FROZEN state as described in {@link
         * https://github.com/WICG/page-lifecycle/blob/master/README.md|Lifecycle API
         * for Web Pages}
         *
         * @returns {Promise} fulfilled after the freeze request is sent, or rejected
         *                    in case the WebDriver command errors
         */
        freeze: function() {
            return window.test_driver_internal.freeze();
        },

        /**
         * Send a sequence of actions
         *
         * This function sends a sequence of actions to the top level window
         * to perform. It is modeled after the behaviour of {@link
         * https://w3c.github.io/webdriver/#actions|WebDriver Actions Command}
         *
         * @param {Array} actions - an array of actions. The format is the same as the actions
                                    property of the WebDriver command {@link
                                    https://w3c.github.io/webdriver/#perform-actions|Perform
                                    Actions} command. Each element is an object representing an
                                    input source and each input source itself has an actions
                                    property detailing the behaviour of that source at each timestep
                                    (or tick). Authors are not expected to construct the actions
                                    sequence by hand, but to use the builder api provided in
                                    testdriver-actions.js
         * @returns {Promise} fufiled after the actions are performed, or rejected in
         *                    the cases the WebDriver command errors
         */
        action_sequence: function(actions) {
            return window.test_driver_internal.action_sequence(actions);
        },

        /**
         * Generates a test report on the current page
         *
         * The generate_test_report function generates a report (to be observed
         * by ReportingObserver) for testing purposes, as described in
         * {@link https://w3c.github.io/reporting/#generate-test-report-command}
         *
         * @returns {Promise} fulfilled after the report is generated, or
         *                    rejected if the report generation fails
         */
        generate_test_report: function(message) {
            return window.test_driver_internal.generate_test_report(message);
        }
    };

    window.test_driver_internal = {
        /**
         * This flag should be set to `true` by any code which implements the
         * internal methods defined below for automation purposes. Doing so
         * allows the library to signal failure immediately when an automated
         * implementation of one of the methods is not available.
         */
        in_automation: false,

        /**
         * Waits for a user-initiated click
         *
         * @param {Element} element - element to be clicked
         * @param {{x: number, y: number} coords - viewport coordinates to click at
         * @returns {Promise} fulfilled after click occurs
         */
        click: function(element, coords) {
            if (this.in_automation) {
                return Promise.reject(new Error('Not implemented'));
            }

            return new Promise(function(resolve, reject) {
                element.addEventListener("click", resolve);
            });
        },

        /**
         * Waits for an element to receive a series of key presses
         *
         * @param {Element} element - element which should receve key presses
         * @param {String} keys - keys to expect
         * @returns {Promise} fulfilled after keys are received or rejected if
         *                    an incorrect key sequence is received
         */
        send_keys: function(element, keys) {
            if (this.in_automation) {
                return Promise.reject(new Error('Not implemented'));
            }

            return new Promise(function(resolve, reject) {
                var seen = "";

                function remove() {
                    element.removeEventListener("keydown", onKeyDown);
                }

                function onKeyDown(event) {
                    if (event.key.length > 1) {
                        return;
                    }

                    seen += event.key;

                    if (keys.indexOf(seen) !== 0) {
                        reject(new Error("Unexpected key sequence: " + seen));
                        remove();
                    } else if (seen === keys) {
                        resolve();
                        remove();
                    }
                }

                element.addEventListener("keydown", onKeyDown);
            });
        },

        /**
         * Freeze the current page
         *
         * @returns {Promise} fulfilled after freeze request is sent, otherwise
         * it gets rejected
         */
        freeze: function() {
            return Promise.reject(new Error("unimplemented"));
        },

        /**
         * Send a sequence of pointer actions
         *
         * @returns {Promise} fufilled after actions are sent, rejected if any actions
         *                    fail
         */
        action_sequence: function(actions) {
            return Promise.reject(new Error("unimplemented"));
        },

        /**
         * Generates a test report on the current page
         *
         * @param {String} message - the message to be contained in the report
         * @returns {Promise} fulfilled after the report is generated, or
         *                    rejected if the report generation fails
         */
        generate_test_report: function(message) {
            return Promise.reject(new Error("unimplemented"));
        }
    };
})();
