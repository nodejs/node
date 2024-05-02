"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.checkStatus = exports.HTTPError = void 0;
class HTTPError extends Error {
    constructor({ status, message, location, }) {
        super(`(${status}) ${message}`);
        this.statusCode = status;
        this.location = location;
    }
}
exports.HTTPError = HTTPError;
const checkStatus = async (response) => {
    if (response.ok) {
        return response;
    }
    else {
        let message = response.statusText;
        const location = response.headers?.get('Location') || undefined;
        const contentType = response.headers?.get('Content-Type');
        // If response type is JSON, try to parse the body for a message
        if (contentType?.includes('application/json')) {
            try {
                await response.json().then((body) => {
                    message = body.message;
                });
            }
            catch (e) {
                // ignore
            }
        }
        throw new HTTPError({
            status: response.status,
            message: message,
            location: location,
        });
    }
};
exports.checkStatus = checkStatus;
