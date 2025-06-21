"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.UnsupportedAlgorithmError = exports.CryptoError = exports.LengthOrHashMismatchError = exports.UnsignedMetadataError = exports.RepositoryError = exports.ValueError = void 0;
// An error about insufficient values
class ValueError extends Error {
}
exports.ValueError = ValueError;
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
