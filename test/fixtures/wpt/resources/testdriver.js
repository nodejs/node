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
     * @namespace {test_driver}
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
         * See `Tracking user activation
         * <https://html.spec.whatwg.org/multipage/interaction.html#tracking-user-activation>`_.
         *
         * @example
         * var mediaElement = document.createElement('video');
         *
         * test_driver.bless('initiate media playback', function () {
         *   mediaElement.play();
         * });
         *
         * @param {String} intent - a description of the action which must be
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
         * If ``element`` isn't inside the
         * viewport, it will be scrolled into view before the click
         * occurs.
         *
         * If ``element`` is from a different browsing context, the
         * command will be run in that context.
         *
         * Matches the behaviour of the `Element Click
         * <https://w3c.github.io/webdriver/#element-click>`_
         * WebDriver command.
         *
         * **Note:** If the element to be clicked does not have a
         * unique ID, the document must not have any DOM mutations
         * made between the function being called and the promise
         * settling.
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
         * Matches the behaviour of the `Delete All Cookies
         * <https://w3c.github.io/webdriver/#delete-all-cookies>`_
         * WebDriver command.
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
         * Get details for all cookies in the current context.
         * See https://w3c.github.io/webdriver/#get-all-cookies
         *
         * @param {WindowProxy} context - Browsing context in which
         *                                to run the call, or null for the current
         *                                browsing context.
         *
         * @returns {Promise} Returns an array of cookies objects as defined in the spec:
         *                    https://w3c.github.io/webdriver/#cookies
         */
        get_all_cookies: function(context=null) {
            return window.test_driver_internal.get_all_cookies(context);
        },

        /**
         * Get details for a cookie in the current context by name if it exists.
         * See https://w3c.github.io/webdriver/#get-named-cookie
         *
         * @param {String} name - The name of the cookie to get.
         * @param {WindowProxy} context - Browsing context in which
         *                                to run the call, or null for the current
         *                                browsing context.
         *
         * @returns {Promise} Returns the matching cookie as defined in the spec:
         *                    https://w3c.github.io/webdriver/#cookies
         *                    Rejected if no such cookie exists.
         */
         get_named_cookie: async function(name, context=null) {
            let cookie = await window.test_driver_internal.get_named_cookie(name, context);
            if (!cookie) {
                throw new Error("no such cookie");
            }
            return cookie;
        },

        /**
         * Send keys to an element.
         *
         * If ``element`` isn't inside the
         * viewport, it will be scrolled into view before the click
         * occurs.
         *
         * If ``element`` is from a different browsing context, the
         * command will be run in that context.
         *
         * To send special keys, send the respective key's codepoint,
         * as defined by `WebDriver
         * <https://w3c.github.io/webdriver/#keyboard-actions>`_.  For
         * example, the "tab" key is represented as "``\uE004``".
         *
         * **Note:** these special-key codepoints are not necessarily
         * what you would expect. For example, <kbd>Esc</kbd> is the
         * invalid Unicode character ``\uE00C``, not the ``\u001B`` Escape
         * character from ASCII.
         *
         * This matches the behaviour of the
         * `Send Keys
         * <https://w3c.github.io/webdriver/#element-send-keys>`_
         * WebDriver command.
         *
         * **Note:** If the element to be clicked does not have a
         * unique ID, the document must not have any DOM mutations
         * made between the function being called and the promise
         * settling.
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
         * the FROZEN state as described in `Lifecycle API for Web Pages
         * <https://github.com/WICG/page-lifecycle/blob/master/README.md>`_.
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
         * Minimizes the browser window.
         *
         * Matches the the behaviour of the `Minimize
         * <https://www.w3.org/TR/webdriver/#minimize-window>`_
         * WebDriver command
         *
         * @param {WindowProxy} context - Browsing context in which
         *                                to run the call, or null for the current
         *                                browsing context.
         *
         * @returns {Promise} fulfilled with the previous {@link
         *                    https://www.w3.org/TR/webdriver/#dfn-windowrect-object|WindowRect}
         *                      value, after the window is minimized.
         */
        minimize_window: function(context=null) {
            return window.test_driver_internal.minimize_window(context);
        },

        /**
         * Restore the window from minimized/maximized state to a given rect.
         *
         * Matches the behaviour of the `Set Window Rect
         * <https://www.w3.org/TR/webdriver/#set-window-rect>`_
         * WebDriver command
         *
         * @param {Object} rect - A {@link
         *                           https://www.w3.org/TR/webdriver/#dfn-windowrect-object|WindowRect}
         * @param {WindowProxy} context - Browsing context in which
         *                                to run the call, or null for the current
         *                                browsing context.
         *
         * @returns {Promise} fulfilled after the window is restored to the given rect.
         */
        set_window_rect: function(rect, context=null) {
            return window.test_driver_internal.set_window_rect(rect, context);
        },

        /**
         * Send a sequence of actions
         *
         * This function sends a sequence of actions to perform.
         *
         * Matches the behaviour of the `Actions
         * <https://w3c.github.io/webdriver/#actions>`_ feature in
         * WebDriver.
         *
         * Authors are encouraged to use the
         * :js:class:`test_driver.Actions` builder rather than
         * invoking this API directly.
         *
         * @param {Array} actions - an array of actions. The format is
         *                          the same as the actions property
         *                          of the `Perform Actions
         *                          <https://w3c.github.io/webdriver/#perform-actions>`_
         *                          WebDriver command. Each element is
         *                          an object representing an input
         *                          source and each input source
         *                          itself has an actions property
         *                          detailing the behaviour of that
         *                          source at each timestep (or
         *                          tick). Authors are not expected to
         *                          construct the actions sequence by
         *                          hand, but to use the builder api
         *                          provided in testdriver-actions.js
         * @param {WindowProxy} context - Browsing context in which
         *                                to run the call, or null for the current
         *                                browsing context.
         *
         * @returns {Promise} fulfilled after the actions are performed, or rejected in
         *                    the cases the WebDriver command errors
         */
        action_sequence: function(actions, context=null) {
            return window.test_driver_internal.action_sequence(actions, context);
        },

        /**
         * Generates a test report on the current page
         *
         * The generate_test_report function generates a report (to be
         * observed by ReportingObserver) for testing purposes.
         *
         * Matches the `Generate Test Report
         * <https://w3c.github.io/reporting/#generate-test-report-command>`_
         * WebDriver command.
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
         * This function simulates a user setting a permission into a
         * particular state.
         *
         * Matches the `Set Permission
         * <https://w3c.github.io/permissions/#set-permission-command>`_
         * WebDriver command.
         *
         * @example
         * await test_driver.set_permission({ name: "background-fetch" }, "denied");
         * await test_driver.set_permission({ name: "push", userVisibleOnly: true }, "granted");
         *
         * @param {PermissionDescriptor} descriptor - a `PermissionDescriptor
         *                              <https://w3c.github.io/permissions/#dom-permissiondescriptor>`_
         *                              dictionary.
         * @param {String} state - the state of the permission
         * @param {WindowProxy} context - Browsing context in which
         *                                to run the call, or null for the current
         *                                browsing context.
         * @returns {Promise} fulfilled after the permission is set, or rejected if setting the
         *                    permission fails
         */
        set_permission: function(descriptor, state, context=null) {
            let permission_params = {
              descriptor,
              state,
            };
            return window.test_driver_internal.set_permission(permission_params, context);
        },

        /**
         * Creates a virtual authenticator
         *
         * This function creates a virtual authenticator for use with
         * the U2F and WebAuthn APIs.
         *
         * Matches the `Add Virtual Authenticator
         * <https://w3c.github.io/webauthn/#sctn-automation-add-virtual-authenticator>`_
         * WebDriver command.
         *
         * @param {Object} config - an `Authenticator Configuration
         *                          <https://w3c.github.io/webauthn/#authenticator-configuration>`_
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
         * This function removes a virtual authenticator that has been
         * created by :js:func:`add_virtual_authenticator`.
         *
         * Matches the `Remove Virtual Authenticator
         * <https://w3c.github.io/webauthn/#sctn-automation-remove-virtual-authenticator>`_
         * WebDriver command.
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
         * Matches the `Add Credential
         * <https://w3c.github.io/webauthn/#sctn-automation-add-credential>`_
         * WebDriver command.
         *
         * @param {String} authenticator_id - the ID of the authenticator
         * @param {Object} credential - A `Credential Parameters
         *                              <https://w3c.github.io/webauthn/#credential-parameters>`_
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
         *
         * Matches the `Get Credentials
         * <https://w3c.github.io/webauthn/#sctn-automation-get-credentials>`_
         * WebDriver command.
         *
         * @param {String} authenticator_id - the ID of the authenticator
         * @param {WindowProxy} context - Browsing context in which
         *                                to run the call, or null for the current
         *                                browsing context.
         *
         * @returns {Promise} fulfilled after the credentials are
         *                    returned, or rejected in the cases the
         *                    WebDriver command errors. Returns an
         *                    array of `Credential Parameters
         *                    <https://w3c.github.io/webauthn/#credential-parameters>`_
         */
        get_credentials: function(authenticator_id, context=null) {
            return window.test_driver_internal.get_credentials(authenticator_id, context=null);
        },

        /**
         * Remove a credential stored in an authenticator
         *
         * Matches the `Remove Credential
         * <https://w3c.github.io/webauthn/#sctn-automation-remove-credential>`_
         * WebDriver command.
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
         * Matches the `Remove All Credentials
         * <https://w3c.github.io/webauthn/#sctn-automation-remove-all-credentials>`_
         * WebDriver command.
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
         *
         * Matches the `Set User Verified
         * <https://w3c.github.io/webauthn/#sctn-automation-set-user-verified>`_
         * WebDriver command.
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
         * Matches the `Set Storage Access
         * <https://privacycg.github.io/storage-access/#set-storage-access-command>`_
         * WebDriver command.
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

        /**
         * Sets the current transaction automation mode for Secure Payment
         * Confirmation.
         *
         * This function places `Secure Payment
         * Confirmation <https://w3c.github.io/secure-payment-confirmation>`_ into
         * an automated 'autoaccept' or 'autoreject' mode, to allow testing
         * without user interaction with the transaction UX prompt.
         *
         * Matches the `Set SPC Transaction Mode
         * <https://w3c.github.io/secure-payment-confirmation/#sctn-automation-set-spc-transaction-mode>`_
         * WebDriver command.
         *
         * @example
         * await test_driver.set_spc_transaction_mode("autoaccept");
         * test.add_cleanup(() => {
         *   return test_driver.set_spc_transaction_mode("none");
         * });
         *
         * // Assumption: `request` is a PaymentRequest with a secure-payment-confirmation
         * // payment method.
         * const response = await request.show();
         *
         * @param {String} mode - The `transaction mode
         *                        <https://w3c.github.io/secure-payment-confirmation/#enumdef-transactionautomationmode>`_
         *                        to set. Must be one of "``none``",
         *                        "``autoaccept``", or
         *                        "``autoreject``".
         * @param {WindowProxy} context - Browsing context in which
         *                                to run the call, or null for the current
         *                                browsing context.
         *
         * @returns {Promise} Fulfilled after the transaction mode has been set,
         *                    or rejected if setting the mode fails.
         */
        set_spc_transaction_mode: function(mode, context=null) {
          return window.test_driver_internal.set_spc_transaction_mode(mode, context);
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

        get_all_cookies: function(context=null) {
            return Promise.reject(new Error("unimplemented"));
        },

        get_named_cookie: function(name, context=null) {
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

        minimize_window: function(context=null) {
            return Promise.reject(new Error("unimplemented"));
        },

        set_window_rect: function(rect, context=null) {
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

        set_spc_transaction_mode: function(mode, context=null) {
            return Promise.reject(new Error("unimplemented"));
        },

    };
})();
