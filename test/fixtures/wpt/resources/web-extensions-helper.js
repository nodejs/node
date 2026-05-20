// testharness file with WebExtensions utilities

/**
 * Loads the WebExtension at the path specified and runs the tests defined in the extension's resources.
 * Listens to messages sent from the user agent and converts the `browser.test` assertions
 * into testharness.js assertions.
 *
 * @param {string} extensionPath - a path to the extension's resources.
 */

setup({ explicit_done: true })
globalThis.runTestsWithWebExtension = function(extensionPath) {
    test_driver.install_web_extension({
        type: "path",
        path: extensionPath
    })
    .then((result) => {
        let test;
        browser.test.onTestStarted.addListener((data) => {
            test = async_test(data.testName)
        })

        browser.test.onTestFinished.addListener((data) => {
            test.step(() => {
                let description = data.message ? `${data.assertionDescription}. ${data.message}` : data.assertionDescription
                assert_true(data.result, description)
            })

            test.done()

            if (!data.result) {
                test.set_status(test.FAIL)
            }

            if (!data.remainingTests) {
                test_driver.uninstall_web_extension(result.extension).then(() => { done() })
            }
        })
    })
}
