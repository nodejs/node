/**
 * @fileoverview Utility to convert gitignore patterns to minimatch.
 * @author Nicholas C. Zakas
 */

/**
 * Converts a gitignore pattern to a minimatch pattern.
 * @param {string} pattern The gitignore pattern to convert. 
 * @returns {string} A minimatch pattern equivalent to `pattern`.
 */
function gitignoreToMinimatch(pattern) {

    if (typeof pattern !== "string") {
        throw new TypeError("Argument must be a string.");
    }

    // Special case: Empty string
    if (!pattern) {
        return pattern;
    }

    // strip off negation to make life easier
    const negated = pattern.startsWith("!");
    let patternToTest = negated ? pattern.slice(1) : pattern;
    let result = patternToTest;
    let leadingSlash = false;

    // strip off leading slash
    if (patternToTest[0] === "/") {
        leadingSlash = true;
        result = patternToTest.slice(1);
    }

    // For the most part, the first character determines what to do
    switch (result[0]) {

        case "*":
            if (patternToTest[1] !== "*") {
                result = "**/" + result;
            }
            break;

        default:
            if (!leadingSlash && !result.includes("/") || result.endsWith("/")) {
                result = "**/" + result;
            }

            // no further changes if the pattern ends with a wildcard
            if (result.endsWith("*") || result.endsWith("?")) {
                break;
            }

            // differentiate between filenames and directory names
            if (!/\.[a-z\d_-]+$/.test(result)) {
                if (!result.endsWith("/")) {
                    result += "/";
                }

                result += "**";
            }
    }

    return negated ? "!" + result : result;

}

export { gitignoreToMinimatch };
