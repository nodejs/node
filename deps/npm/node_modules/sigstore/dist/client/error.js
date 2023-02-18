"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.checkStatus = exports.HTTPError = void 0;
class HTTPError extends Error {
    constructor(response) {
        super(`HTTP Error: ${response.status} ${response.statusText}`);
        this.response = response;
        this.statusCode = response.status;
        this.location = response.headers?.get('Location') || undefined;
    }
}
exports.HTTPError = HTTPError;
const checkStatus = (response) => {
    if (response.ok) {
        return response;
    }
    else {
        throw new HTTPError(response);
    }
};
exports.checkStatus = checkStatus;
