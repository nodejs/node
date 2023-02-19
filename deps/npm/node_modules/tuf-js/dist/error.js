"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.DownloadHTTPError = exports.DownloadLengthMismatchError = exports.DownloadError = exports.UnsupportedAlgorithmError = exports.CryptoError = exports.LengthOrHashMismatchError = exports.ExpiredMetadataError = exports.EqualVersionError = exports.BadVersionError = exports.UnsignedMetadataError = exports.RepositoryError = exports.PersistError = exports.RuntimeError = exports.ValueError = void 0;
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
// An error about metadata object with insufficient threshold of signatures.
class UnsignedMetadataError extends RepositoryError {
}
exports.UnsignedMetadataError = UnsignedMetadataError;
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
// An error while checking the length and hash values of an object.
class LengthOrHashMismatchError extends RepositoryError {
}
exports.LengthOrHashMismatchError = LengthOrHashMismatchError;
class CryptoError extends Error {
}
exports.CryptoError = CryptoError;
class UnsupportedAlgorithmError extends CryptoError {
}
exports.UnsupportedAlgorithmError = UnsupportedAlgorithmError;
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
    constructor(message, statusCode) {
        super(message);
        this.statusCode = statusCode;
    }
}
exports.DownloadHTTPError = DownloadHTTPError;
