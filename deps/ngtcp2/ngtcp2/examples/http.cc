/*
 * ngtcp2
 *
 * Copyright (c) 2017 ngtcp2 contributors
 * Copyright (c) 2012 nghttp2 contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "http.h"

using namespace std::literals;

namespace ngtcp2 {

namespace http {

std::string_view get_reason_phrase(unsigned int status_code) {
  switch (status_code) {
  case 100:
    return "Continue"sv;
  case 101:
    return "Switching Protocols"sv;
  case 200:
    return "OK"sv;
  case 201:
    return "Created"sv;
  case 202:
    return "Accepted"sv;
  case 203:
    return "Non-Authoritative Information"sv;
  case 204:
    return "No Content"sv;
  case 205:
    return "Reset Content"sv;
  case 206:
    return "Partial Content"sv;
  case 300:
    return "Multiple Choices"sv;
  case 301:
    return "Moved Permanently"sv;
  case 302:
    return "Found"sv;
  case 303:
    return "See Other"sv;
  case 304:
    return "Not Modified"sv;
  case 305:
    return "Use Proxy"sv;
  // case 306: return "(Unused)"sv;
  case 307:
    return "Temporary Redirect"sv;
  case 308:
    return "Permanent Redirect"sv;
  case 400:
    return "Bad Request"sv;
  case 401:
    return "Unauthorized"sv;
  case 402:
    return "Payment Required"sv;
  case 403:
    return "Forbidden"sv;
  case 404:
    return "Not Found"sv;
  case 405:
    return "Method Not Allowed"sv;
  case 406:
    return "Not Acceptable"sv;
  case 407:
    return "Proxy Authentication Required"sv;
  case 408:
    return "Request Timeout"sv;
  case 409:
    return "Conflict"sv;
  case 410:
    return "Gone"sv;
  case 411:
    return "Length Required"sv;
  case 412:
    return "Precondition Failed"sv;
  case 413:
    return "Payload Too Large"sv;
  case 414:
    return "URI Too Long"sv;
  case 415:
    return "Unsupported Media Type"sv;
  case 416:
    return "Requested Range Not Satisfiable"sv;
  case 417:
    return "Expectation Failed"sv;
  case 421:
    return "Misdirected Request"sv;
  case 426:
    return "Upgrade Required"sv;
  case 428:
    return "Precondition Required"sv;
  case 429:
    return "Too Many Requests"sv;
  case 431:
    return "Request Header Fields Too Large"sv;
  case 451:
    return "Unavailable For Legal Reasons"sv;
  case 500:
    return "Internal Server Error"sv;
  case 501:
    return "Not Implemented"sv;
  case 502:
    return "Bad Gateway"sv;
  case 503:
    return "Service Unavailable"sv;
  case 504:
    return "Gateway Timeout"sv;
  case 505:
    return "HTTP Version Not Supported"sv;
  case 511:
    return "Network Authentication Required"sv;
  default:
    return ""sv;
  }
}

} // namespace http

} // namespace ngtcp2
