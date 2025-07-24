"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.DownloadHTTPError = exports.DownloadLengthMismatchError = exports.DownloadError = exports.ExpiredMetadataError = exports.EqualVersionError = exports.BadVersionError = exports.RepositoryError = exports.PersistError = exports.RuntimeError = exports.ValueError = void 0;
// An error about insufficient values
class ValueError extends Error {
}
exports.ValueError = ValueError;
class RuntimeError extends Error {
}
exports.RuntimeError = RuntimeError;
class PersistError extends Error {
}
exports.PersistError = PersistError;
// An error with a repository's state, such as a missing file.
// It covers all exceptions that come from the repository side when
// looking from the perspective of users of metadata API or ngclient.
class RepositoryError extends Error {
}
exports.RepositoryError = RepositoryError;
// An error for metadata that contains an invalid version number.
class BadVersionError extends RepositoryError {
}
exports.BadVersionError = BadVersionError;
// An error for metadata containing a previously verified version number.
class EqualVersionError extends BadVersionError {
}
exports.EqualVersionError = EqualVersionError;
// Indicate that a TUF Metadata file has expired.
class ExpiredMetadataError extends RepositoryError {
}
exports.ExpiredMetadataError = ExpiredMetadataError;
//----- Download Errors -------------------------------------------------------
// An error occurred while attempting to download a file.
class DownloadError extends Error {
}
exports.DownloadError = DownloadError;
// Indicate that a mismatch of lengths was seen while downloading a file
class DownloadLengthMismatchError extends DownloadError {
}
exports.DownloadLengthMismatchError = DownloadLengthMismatchError;
// Returned by FetcherInterface implementations for HTTP errors.
class DownloadHTTPError extends DownloadError {
    statusCode;
    constructor(message, statusCode) {
        super(message);
        this.statusCode = statusCode;
    }
}
exports.DownloadHTTPError = DownloadHTTPError;
