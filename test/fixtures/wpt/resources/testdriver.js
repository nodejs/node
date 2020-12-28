(function() {
    "use strict";
    var idCounter = 0;
    let testharness_context = null;

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
        let elementDocument = element.ownerDocument;
        if (!elementDocument.contains(element)) {
            return [];
        }

        var rectangles = element.getClientRects();

        if (rectangles.length === 0) {
            return [];
        }

        var centerPoint = getInViewCenterPoint(rectangles[0]);

        if ("elementsFromPoint" in elementDocument) {
            return elementDocument.elementsFromPoint(centerPoint[0], centerPoint[1]);
        } else if ("msElementsFromPoint" in elementDocument) {
            var rv = elementDocument.msElementsFromPoint(centerPoint[0], centerPoint[1]);
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
         * Set the context in which testharness.js is loaded
         *
         * @param {WindowProxy} context - the window containing testharness.js
         **/
        set_test_context: function(context) {
          if (window.test_driver_internal.set_test_context) {
            window.test_driver_internal.set_test_context(context);
          }
          testharness_context = context;
        },

        /**
         * postMessage to the context containing testharness.js
         *
         * @param {Object} msg - the data to POST
         **/
        message_test: function(msg) {
            let target = testharness_context;
            if (testharness_context === null) {
                target = window;
            }
            target.postMessage(msg, "*");
        },

        /**
         * Trigger user interaction in order to grant additional privileges to
         * a provided function.
         *
         * https://html.spec.whatwg.org/#triggered-by-user-activation
         *
         * @param {String} intent - a description of the action which much be
         *                          triggered by user interaction
         * @param {Function} action - code requiring escalated privileges
         * @param {WindowProxy} context - Browsing context in which
         *                                to run the call, or null for the current
         *                                browsing context.
         *
         * @returns {Promise} fulfilled following user interaction and
         *                    execution of the provided `action` function;
         *                    rejected if interaction fails or the provided
         *                    function throws an error
         */
        bless: function(intent, action, context=null) {
            let contextDocument = context ? context.document : document;
            var button = contextDocument.createElement("button");
            button.innerHTML = "This test requires user interaction.<br />" +
                "Please click here to allow " + intent + ".";
            button.id = "wpt-test-driver-bless-" + (idCounter += 1);
            const elem = contextDocument.body || contextDocument.documentElement;
            elem.appendChild(button);

            let wait_click = new Promise(resolve => button.addEventListener("click", resolve));

            return test_driver.click(button)
                .then(wait_click)
                .then(function() {
                    button.remove();

                    if (typeof action === "function") {
                        return action();
                    }
                    return null;
                });
        },

        /**
         * Triggers a user-initiated click
         *
         * This matches the behaviour of the {@link
         * https://w3c.github.io/webdriver/#element-click|WebDriver
         * Element Click command}.
         *
         * @param {Element} element - element to be clicked
         * @returns {Promise} fulfilled after click occurs, or rejected in
         *                    the cases the WebDriver command errors
         */
        click: function(element) {
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
         * Deletes all cookies.
         *
         * This matches the behaviour of the {@link
         * https://w3c.github.io/webdriver/#delete-all-cookies|WebDriver
         * Delete All Cookies command}.
         *
         * @param {WindowProxy} context - Browsing context in which
         *                                to run the call, or null for the current
         *                                browsing context.
         *
         * @returns {Promise} fulfilled after cookies are deleted, or rejected in
         *                    the cases the WebDriver command errors
         */
        delete_all_cookies: function(context=null) {
            return window.test_driver_internal.delete_all_cookies(context);
        },

        /**
         * Send keys to an element
         *
         * This matches the behaviour of the {@link
         * https://w3c.github.io/webdriver/#element-send-keys|WebDriver
         * Send Keys command}.
         *
         * @param {Element} element - element to send keys to
         * @param {String} keys - keys to send to the element
         * @returns {Promise} fulfilled after keys are sent, or rejected in
         *                    the cases the WebDriver command errors
         */
        send_keys: function(element, keys) {
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
         * @param {WindowProxy} context - Browsing context in which
         *                                to run the call, or null for the current
         *                                browsing context.
         *
         * @returns {Promise} fulfilled after the freeze request is sent, or rejected
         *                    in case the WebDriver command errors
         */
        freeze: function(context=null) {
            return window.test_driver_internal.freeze();
        },

        /**
         * Send a sequence of actions
         *
         * This function sends a sequence of actions
         * to perform. It is modeled after the behaviour of {@link
         * https://w3c.github.io/webdriver/#actions|WebDriver Actions Command}
         *
         * @param {Array} actions - an array of actions. The format is the same as the actions
         *                          property of the WebDriver command {@link
         *                          https://w3c.github.io/webdriver/#perform-actions|Perform
         *                          Actions} command. Each element is an object representing an
         *                          input source and each input source itself has an actions
         *                          property detailing the behaviour of that source at each timestep
         *                          (or tick). Authors are not expected to construct the actions
         *                          sequence by hand, but to use the builder api provided in
         *                          testdriver-actions.js
         * @param {WindowProxy} context - Browsing context in which
         *                                to run the call, or null for the current
         *                                browsing context.
         *
         * @returns {Promise} fufiled after the actions are performed, or rejected in
         *                    the cases the WebDriver command errors
         */
        action_sequence: function(actions, context=null) {
            return window.test_driver_internal.action_sequence(actions, context);
        },

        /**
         * Generates a test report on the current page
         *
         * The generate_test_report function generates a report (to be observed
         * by ReportingObserver) for testing purposes, as described in
         * {@link https://w3c.github.io/reporting/#generate-test-report-command}
         *
         * @param {WindowProxy} context - Browsing context in which
         *                                to run the call, or null for the current
         *                                browsing context.
         *
         * @returns {Promise} fulfilled after the report is generated, or
         *                    rejected if the report generation fails
         */
        generate_test_report: function(message, context=null) {
            return window.test_driver_internal.generate_test_report(message, context);
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
         * @param {WindowProxy} context - Browsing context in which
         *                                to run the call, or null for the current
         *                                browsing context.
         *
         * The above params are used to create a [PermissionSetParameters]{@link
         * https://w3c.github.io/permissions/#dictdef-permissionsetparameters} object
         *
         * @returns {Promise} fulfilled after the permission is set, or rejected if setting the
         *                    permission fails
         */
        set_permission: function(descriptor, state, one_realm, context=null) {
            let permission_params = {
              descriptor,
              state,
              oneRealm: one_realm,
            };
            return window.test_driver_internal.set_permission(permission_params, context);
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
         * @param {WindowProxy} context - Browsing context in which
         *                                to run the call, or null for the current
         *                                browsing context.
         *
         * @returns {Promise} fulfilled after the authenticator is added, or
         *                    rejected in the cases the WebDriver command
         *                    errors. Returns the ID of the authenticator
         */
        add_virtual_authenticator: function(config, context=null) {
            return window.test_driver_internal.add_virtual_authenticator(config, context);
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
         * @param {WindowProxy} context - Browsing context in which
         *                                to run the call, or null for the current
         *                                browsing context.
         *
         * @returns {Promise} fulfilled after the authenticator is removed, or
         *                    rejected in the cases the WebDriver command
         *                    errors
         */
        remove_virtual_authenticator: function(authenticator_id, context=null) {
            return window.test_driver_internal.remove_virtual_authenticator(authenticator_id, context);
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
         * @param {WindowProxy} context - Browsing context in which
         *                                to run the call, or null for the current
         *                                browsing context.
         *
         * @returns {Promise} fulfilled after the credential is added, or
         *                    rejected in the cases the WebDriver command
         *                    errors
         */
        add_credential: function(authenticator_id, credential, context=null) {
            return window.test_driver_internal.add_credential(authenticator_id, credential, context);
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
         * @param {WindowProxy} context - Browsing context in which
         *                                to run the call, or null for the current
         *                                browsing context.
         *
         * @returns {Promise} fulfilled after the credentials are returned, or
         *                    rejected in the cases the WebDriver command
         *                    errors. Returns an array of [Credential
         *                    Parameters]{@link
         *                    https://w3c.github.io/webauthn/#credential-parameters}
         */
        get_credentials: function(authenticator_id, context=null) {
            return window.test_driver_internal.get_credentials(authenticator_id, context=null);
        },

        /**
         * Remove a credential stored in an authenticator
         *
         * https://w3c.github.io/webauthn/#sctn-automation-remove-credential
         *
         * @param {String} authenticator_id - the ID of the authenticator
         * @param {String} credential_id - the ID of the credential
         * @param {WindowProxy} context - Browsing context in which
         *                                to run the call, or null for the current
         *                                browsing context.
         *
         * @returns {Promise} fulfilled after the credential is removed, or
         *                    rejected in the cases the WebDriver command
         *                    errors.
         */
        remove_credential: function(authenticator_id, credential_id, context=null) {
            return window.test_driver_internal.remove_credential(authenticator_id, credential_id, context);
        },

        /**
         * Removes all the credentials stored in a virtual authenticator
         *
         * https://w3c.github.io/webauthn/#sctn-automation-remove-all-credentials
         *
         * @param {String} authenticator_id - the ID of the authenticator
         * @param {WindowProxy} context - Browsing context in which
         *                                to run the call, or null for the current
         *                                browsing context.
         *
         * @returns {Promise} fulfilled after the credentials are removed, or
         *                    rejected in the cases the WebDriver command
         *                    errors.
         */
        remove_all_credentials: function(authenticator_id, context=null) {
            return window.test_driver_internal.remove_all_credentials(authenticator_id, context);
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
         * @param {WindowProxy} context - Browsing context in which
         *                                to run the call, or null for the current
         *                                browsing context.
         */
        set_user_verified: function(authenticator_id, uv, context=null) {
            return window.test_driver_internal.set_user_verified(authenticator_id, uv, context);
        },

        /**
         * Sets the storage access rule for an origin when embedded
         * in a third-party context.
         *
         * {@link https://privacycg.github.io/storage-access/#set-storage-access-command}
         *
         * @param {String} origin - A third-party origin to block or allow.
         *                          May be "*" to indicate all origins.
         * @param {String} embedding_origin - an embedding (first-party) origin
         *                                    on which {origin}'s access should
         *                                    be blocked or allowed.
         *                                    May be "*" to indicate all origins.
         * @param {String} state - The storage access setting.
         *                         Must be either "allowed" or "blocked".
         * @param {WindowProxy} context - Browsing context in which
         *                                to run the call, or null for the current
         *                                browsing context.
         *
         * @returns {Promise} Fulfilled after the storage access rule has been
         *                    set, or rejected if setting the rule fails.
         */
        set_storage_access: function(origin, embedding_origin, state, context=null) {
            if (state !== "allowed" && state !== "blocked") {
                throw new Error("storage access status must be 'allowed' or 'blocked'");
            }
            const blocked = state === "blocked";
            return window.test_driver_internal.set_storage_access(origin, embedding_origin, blocked, context);
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

        click: function(element, coords) {
            if (this.in_automation) {
                return Promise.reject(new Error('Not implemented'));
            }

            return new Promise(function(resolve, reject) {
                element.addEventListener("click", resolve);
            });
        },

        delete_all_cookies: function(context=null) {
            return Promise.reject(new Error("unimplemented"));
        },

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

        freeze: function(context=null) {
            return Promise.reject(new Error("unimplemented"));
        },

        action_sequence: function(actions, context=null) {
            return Promise.reject(new Error("unimplemented"));
        },

        generate_test_report: function(message, context=null) {
            return Promise.reject(new Error("unimplemented"));
        },


        set_permission: function(permission_params, context=null) {
            return Promise.reject(new Error("unimplemented"));
        },

        add_virtual_authenticator: function(config, context=null) {
            return Promise.reject(new Error("unimplemented"));
        },

        remove_virtual_authenticator: function(authenticator_id, context=null) {
            return Promise.reject(new Error("unimplemented"));
        },

        add_credential: function(authenticator_id, credential, context=null) {
            return Promise.reject(new Error("unimplemented"));
        },

        get_credentials: function(authenticator_id, context=null) {
            return Promise.reject(new Error("unimplemented"));
        },

        remove_credential: function(authenticator_id, credential_id, context=null) {
            return Promise.reject(new Error("unimplemented"));
        },

        remove_all_credentials: function(authenticator_id, context=null) {
            return Promise.reject(new Error("unimplemented"));
        },

        set_user_verified: function(authenticator_id, uv, context=null) {
            return Promise.reject(new Error("unimplemented"));
        },

        set_storage_access: function(origin, embedding_origin, blocked, context=null) {
            return Promise.reject(new Error("unimplemented"));
        },
    };
})();
