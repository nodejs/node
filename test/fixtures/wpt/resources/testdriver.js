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
        },

        /**
         * Sets the state of a permission
         *
         * This function simulates a user setting a permission into a particular state as described
         * in {@link https://w3c.github.io/permissions/#set-permission-command}
         *
         * @param {Object} descriptor - a [PermissionDescriptor]{@link
         *                              https://w3c.github.io/permissions/#dictdef-permissiondescriptor}
         *                              object
         * @param {String} state - the state of the permission
         * @param {boolean} one_realm - Optional. Whether the permission applies to only one realm
         *
         * The above params are used to create a [PermissionSetParameters]{@link
         * https://w3c.github.io/permissions/#dictdef-permissionsetparameters} object
         *
         * @returns {Promise} fulfilled after the permission is set, or rejected if setting the
         *                    permission fails
         */
        set_permission: function(descriptor, state, one_realm) {
            let permission_params = {
              descriptor,
              state,
              oneRealm: one_realm,
            };
            return window.test_driver_internal.set_permission(permission_params);
        },

        /**
         * Creates a virtual authenticator
         *
         * This function creates a virtual authenticator for use with the U2F
         * and WebAuthn APIs as described in {@link
         * https://w3c.github.io/webauthn/#sctn-automation-add-virtual-authenticator}
         *
         * @param {Object} config - an [Authenticator Configuration]{@link
         *                          https://w3c.github.io/webauthn/#authenticator-configuration}
         *                          object
         * @returns {Promise} fulfilled after the authenticator is added, or
         *                    rejected in the cases the WebDriver command
         *                    errors. Returns the ID of the authenticator
         */
        add_virtual_authenticator: function(config) {
            return window.test_driver_internal.add_virtual_authenticator(config);
        },

        /**
         * Removes a virtual authenticator
         *
         * This function removes a virtual authenticator that has been created
         * by add_virtual_authenticator
         * https://w3c.github.io/webauthn/#sctn-automation-remove-virtual-authenticator
         *
         * @param {String} authenticator_id - the ID of the authenticator to be
         *                                    removed.
         *
         * @returns {Promise} fulfilled after the authenticator is removed, or
         *                    rejected in the cases the WebDriver command
         *                    errors
         */
        remove_virtual_authenticator: function(authenticator_id) {
            return window.test_driver_internal.remove_virtual_authenticator(authenticator_id);
        },

        /**
         * Adds a credential to a virtual authenticator
         *
         * https://w3c.github.io/webauthn/#sctn-automation-add-credential
         *
         * @param {String} authenticator_id - the ID of the authenticator
         * @param {Object} credential - A [Credential Parameters]{@link
         *                              https://w3c.github.io/webauthn/#credential-parameters}
         *                              object
         *
         * @returns {Promise} fulfilled after the credential is added, or
         *                    rejected in the cases the WebDriver command
         *                    errors
         */
        add_credential: function(authenticator_id, credential) {
            return window.test_driver_internal.add_credential(authenticator_id, credential);
        },

        /**
         * Gets all the credentials stored in an authenticator
         *
         * This function retrieves all the credentials (added via the U2F API,
         * WebAuthn, or the add_credential function) stored in a virtual
         * authenticator
         * https://w3c.github.io/webauthn/#sctn-automation-get-credentials
         *
         * @param {String} authenticator_id - the ID of the authenticator
         *
         * @returns {Promise} fulfilled after the credentials are returned, or
         *                    rejected in the cases the WebDriver command
         *                    errors. Returns an array of [Credential
         *                    Parameters]{@link
         *                    https://w3c.github.io/webauthn/#credential-parameters}
         */
        get_credentials: function(authenticator_id) {
            return window.test_driver_internal.get_credentials(authenticator_id);
        },

        /**
         * Remove a credential stored in an authenticator
         *
         * https://w3c.github.io/webauthn/#sctn-automation-remove-credential
         *
         * @param {String} authenticator_id - the ID of the authenticator
         * @param {String} credential_id - the ID of the credential
         *
         * @returns {Promise} fulfilled after the credential is removed, or
         *                    rejected in the cases the WebDriver command
         *                    errors.
         */
        remove_credential: function(authenticator_id, credential_id) {
            return window.test_driver_internal.remove_credential(authenticator_id, credential_id);
        },

        /**
         * Removes all the credentials stored in a virtual authenticator
         *
         * https://w3c.github.io/webauthn/#sctn-automation-remove-all-credentials
         *
         * @param {String} authenticator_id - the ID of the authenticator
         *
         * @returns {Promise} fulfilled after the credentials are removed, or
         *                    rejected in the cases the WebDriver command
         *                    errors.
         */
        remove_all_credentials: function(authenticator_id) {
            return window.test_driver_internal.remove_all_credentials(authenticator_id);
        },

        /**
         * Sets the User Verified flag on an authenticator
         *
         * Sets whether requests requiring user verification will succeed or
         * fail on a given virtual authenticator
         * https://w3c.github.io/webauthn/#sctn-automation-set-user-verified
         *
         * @param {String} authenticator_id - the ID of the authenticator
         * @param {boolean} uv - the User Verified flag
         */
        set_user_verified: function(authenticator_id, uv) {
            return window.test_driver_internal.set_user_verified(authenticator_id, uv);
        },
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
        },


        /**
         * Sets the state of a permission
         *
         * This function simulates a user setting a permission into a particular state as described
         * in {@link https://w3c.github.io/permissions/#set-permission-command}
         *
         * @param {Object} permission_params - a [PermissionSetParameters]{@lint
         *                                     https://w3c.github.io/permissions/#dictdef-permissionsetparameters}
         *                                     object
         * @returns {Promise} fulfilled after the permission is set, or rejected if setting the
         *                    permission fails
         */
        set_permission: function(permission_params) {
            return Promise.reject(new Error("unimplemented"));
        },

        /**
         * Creates a virtual authenticator
         *
         * @param {Object} config - the authenticator configuration
         * @returns {Promise} fulfilled after the authenticator is added, or
         *                    rejected in the cases the WebDriver command
         *                    errors.
         */
        add_virtual_authenticator: function(config) {
            return Promise.reject(new Error("unimplemented"));
        },

        /**
         * Removes a virtual authenticator
         *
         * @param {String} authenticator_id - the ID of the authenticator to be
         *                                    removed.
         *
         * @returns {Promise} fulfilled after the authenticator is removed, or
         *                    rejected in the cases the WebDriver command
         *                    errors
         */
        remove_virtual_authenticator: function(authenticator_id) {
            return Promise.reject(new Error("unimplemented"));
        },

        /**
         * Adds a credential to a virtual authenticator
         *
         * @param {String} authenticator_id - the ID of the authenticator
         * @param {Object} credential - A [Credential Parameters]{@link
         *                              https://w3c.github.io/webauthn/#credential-parameters}
         *                              object
         *
         * @returns {Promise} fulfilled after the credential is added, or
         *                    rejected in the cases the WebDriver command
         *                    errors
         *
         */
        add_credential: function(authenticator_id, credential) {
            return Promise.reject(new Error("unimplemented"));
        },

        /**
         * Gets all the credentials stored in an authenticator
         *
         * @param {String} authenticator_id - the ID of the authenticator
         *
         * @returns {Promise} fulfilled after the credentials are returned, or
         *                    rejected in the cases the WebDriver command
         *                    errors. Returns an array of [Credential
         *                    Parameters]{@link
         *                    https://w3c.github.io/webauthn/#credential-parameters}
         *
         */
        get_credentials: function(authenticator_id) {
            return Promise.reject(new Error("unimplemented"));
        },

        /**
         * Remove a credential stored in an authenticator
         *
         * @param {String} authenticator_id - the ID of the authenticator
         * @param {String} credential_id - the ID of the credential
         *
         * @returns {Promise} fulfilled after the credential is removed, or
         *                    rejected in the cases the WebDriver command
         *                    errors.
         *
         */
        remove_credential: function(authenticator_id, credential_id) {
            return Promise.reject(new Error("unimplemented"));
        },

        /**
         * Removes all the credentials stored in a virtual authenticator
         *
         * @param {String} authenticator_id - the ID of the authenticator
         *
         * @returns {Promise} fulfilled after the credentials are removed, or
         *                    rejected in the cases the WebDriver command
         *                    errors.
         *
         */
        remove_all_credentials: function(authenticator_id) {
            return Promise.reject(new Error("unimplemented"));
        },

        /**
         * Sets the User Verified flag on an authenticator
         *
         * @param {String} authenticator_id - the ID of the authenticator
         * @param {boolean} uv - the User Verified flag
         *
         */
        set_user_verified: function(authenticator_id, uv) {
            return Promise.reject(new Error("unimplemented"));
        },
    };
})();
