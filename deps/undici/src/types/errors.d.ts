import {IncomingHttpHeaders} from "http";

export = Errors
import { SocketInfo } from './client'

declare namespace Errors {
  export class UndiciError extends Error { }

  /** A header exceeds the `headersTimeout` option. */
  export class HeadersTimeoutError extends UndiciError {
    name: 'HeadersTimeoutError';
    code: 'UND_ERR_HEADERS_TIMEOUT';
  }

  /** A body exceeds the `bodyTimeout` option. */
  export class BodyTimeoutError extends UndiciError {
    name: 'BodyTimeoutError';
    code: 'UND_ERR_BODY_TIMEOUT';
  }

  export class ResponseStatusCodeError extends UndiciError {
    name: 'ResponseStatusCodeError';
    code: 'UND_ERR_RESPONSE_STATUS_CODE';
    body: null | Record<string, any> | string
    status: number
    statusCode: number
    headers: IncomingHttpHeaders | string[] | null;
  }

  /** A socket exceeds the `socketTimeout` option. */
  export class SocketTimeoutError extends UndiciError {
    name: 'SocketTimeoutError';
    code: 'UND_ERR_SOCKET_TIMEOUT';
  }

  /** Passed an invalid argument. */
  export class InvalidArgumentError extends UndiciError {
    name: 'InvalidArgumentError';
    code: 'UND_ERR_INVALID_ARG';
  }

  /** Returned an invalid value. */
  export class InvalidReturnError extends UndiciError {
    name: 'InvalidReturnError';
    code: 'UND_ERR_INVALID_RETURN_VALUE';
  }

  /** The request has been aborted by the user. */
  export class RequestAbortedError extends UndiciError {
    name: 'RequestAbortedError';
    code: 'UND_ERR_ABORTED';
  }

  /** Expected error with reason. */
  export class InformationalError extends UndiciError {
    name: 'InformationalError';
    code: 'UND_ERR_INFO';
  }

  /** Body does not match content-length header. */
  export class RequestContentLengthMismatchError extends UndiciError {
    name: 'RequestContentLengthMismatchError';
    code: 'UND_ERR_REQ_CONTENT_LENGTH_MISMATCH';
  }

  /** Trying to use a destroyed client. */
  export class ClientDestroyedError extends UndiciError {
    name: 'ClientDestroyedError';
    code: 'UND_ERR_DESTROYED';
  }

  /** Trying to use a closed client. */
  export class ClientClosedError extends UndiciError {
    name: 'ClientClosedError';
    code: 'UND_ERR_CLOSED';
  }

  /** There is an error with the socket. */
  export class SocketError extends UndiciError {
    name: 'SocketError';
    code: 'UND_ERR_SOCKET';
    socket: SocketInfo | null
  }

  /** Encountered unsupported functionality. */
  export class NotSupportedError extends UndiciError {
    name: 'NotSupportedError';
    code: 'UND_ERR_NOT_SUPPORTED';
  }
}
