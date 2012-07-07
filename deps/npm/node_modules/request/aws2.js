var crypto = require('crypto')

// The Authentication Header
// 
// The Amazon S3 REST API uses the standard HTTPAuthorization header to pass authentication information. (The name of the standard header is unfortunate because it carries authentication information, not authorization).Under the Amazon S3 authentication scheme, the Authorization header has the following form.
// 
// Authorization: AWS AWSAccessKeyId:Signature
// 
// Developers are issued an AWS Access Key ID and AWS SecretAccess Key when they register. For request authentication, theAWSAccessKeyId element identifies the secret key that was used to compute the signature, and (indirectly) the developer making the request.
// 
// The Signature element is the RFC 2104HMAC-SHA1 of selected elements from the request, and so theSignature part of the Authorization header will vary from request to request. If the request signature calculated by the system matches theSignature included with the request, then the requester will have demonstrated possession to the AWSSecret Access Key. The request will then be processed under the identity, and with the authority, of the developer to whom the key was issued.
// 
// Following is pseudo-grammar that illustrates the construction of the Authorization request header (\nmeans the Unicode code point U+000A commonly called newline).

function authorizationHeader (accessKey) {
  // Authorization = "AWS" + " " + AWSAccessKeyId + ":" + Signature;
  
  var authorization = 'AWS' + " " + accessKey + ":" + signature()
  
  return authorization
}

// 

function signature (secret, verb, md5, contenttype, date, amzheaders, bucket, path) {
  // Signature = Base64( HMAC-SHA1( UTF-8-Encoding-Of( YourSecretAccessKeyID, StringToSign ) ) );
  
  function encodeSignature (stringToSign) {
    return crypto.createHash('sha1').update(stringToSign).digest('base64')
  }
  
  // 
  // StringToSign = HTTP-Verb + "\n" +
  //  Content-MD5 + "\n" +
  //  Content-Type + "\n" +
  //  Date + "\n" +
  //  CanonicalizedAmzHeaders +
  //  CanonicalizedResource;
  
  function compileStringToSign () {
    var s = 
      verb + '\n'
      (md5 || '') + '\n'
      (contenttype || '') + '\n'
      date.toUTCString() + '\n'
      canonicalizeAmzHeaders(amzheaders) + 
      canonicalizeResource()
    return s
  }
  
  // 
  // CanonicalizedResource = [ "/" + Bucket ] +
  //  <HTTP-Request-URI, from the protocol name up to the query string> +
  //  [ sub-resource, if present. For example "?acl", "?location", "?logging", or "?torrent"];
  
  function canonicalizeResource () {
    return '/' + bucket + path
  }
  
  // 
  // CanonicalizedAmzHeaders = <described below>
  // 
  // HMAC-SHA1 is an algorithm defined by RFC 2104 (go to RFC 2104 - Keyed-Hashing for Message Authentication ). The algorithm takes as input two byte-strings: a key and a message. For Amazon S3 Request authentication, use your AWS Secret Access Key (YourSecretAccessKeyID) as the key, and the UTF-8 encoding of the StringToSign as the message. The output of HMAC-SHA1 is also a byte string, called the digest. The Signature request parameter is constructed by Base64 encoding this digest.
  // Request Canonicalization for Signing
  // 
  // Recall that when the system receives an authenticated request, it compares the computed request signature with the signature provided in the request in StringToSign. For that reason, you must compute the signature using the same method used by Amazon S3. We call the process of putting a request in an agreed-upon form for signing "canonicalization".
  
}



// Constructing the CanonicalizedResource Element
// 
// CanonicalizedResource represents the Amazon S3 resource targeted by the request. Construct it for a REST request as follows:
// 
// Launch Process
// 
// 1
//  
// 
// Start with the empty string ("").
// 
// 2
//  
// 
// If the request specifies a bucket using the HTTP Host header (virtual hosted-style), append the bucket name preceded by a "/" (e.g., "/bucketname"). For path-style requests and requests that don't address a bucket, do nothing. For more information on virtual hosted-style requests, see Virtual Hosting of Buckets.
// 
// 3
//  
// 
// Append the path part of the un-decoded HTTP Request-URI, up-to but not including the query string.
// 
// 4
//  
// 
// If the request addresses a sub-resource, like ?versioning, ?location, ?acl, ?torrent, ?lifecycle, or ?versionid append the sub-resource, its value if it has one, and the question mark. Note that in case of multiple sub-resources, sub-resources must be lexicographically sorted by sub-resource name and separated by '&'. e.g. ?acl&versionId=value.
// 
// The list of sub-resources that must be included when constructing the CanonicalizedResource Element are: acl, lifecycle, location, logging, notification, partNumber, policy, requestPayment, torrent, uploadId, uploads, versionId, versioning, versions and website.
// 
// If the request specifies query string parameters overriding the response header values (see Get Object), append the query string parameters, and its values. When signing you do not encode these values. However, when making the request, you must encode these parameter values. The query string parameters in a GET request include response-content-type, response-content-language, response-expires, response-cache-control, response-content-disposition, response-content-encoding.
// 
// The delete query string parameter must be including when creating the CanonicalizedResource for a Multi-Object Delete request.
// 
// Elements of the CanonicalizedResource that come from the HTTP Request-URI should be signed literally as they appear in the HTTP request, including URL-Encoding meta characters.
// 
// The CanonicalizedResource might be different than the HTTP Request-URI. In particular, if your request uses the HTTP Host header to specify a bucket, the bucket does appear in the HTTP Request-URI. However, the CanonicalizedResource continues to include the bucket. Query string parameters might also appear in the Request-URI but are not included in CanonicalizedResource. For more information, see Virtual Hosting of Buckets.
// Constructing the CanonicalizedAmzHeaders Element
// 
// To construct the CanonicalizedAmzHeaders part of StringToSign, select all HTTP request headers that start with 'x-amz-' (using a case-insensitive comparison) and use the following process.
// 
// CanonicalizedAmzHeaders Process
// 1  Convert each HTTP header name to lower-case. For example, 'X-Amz-Date' becomes 'x-amz-date'.
// 2  Sort the collection of headers lexicographically by header name.
// 3  Combine header fields with the same name into one "header-name:comma-separated-value-list" pair as prescribed by RFC 2616, section 4.2, without any white-space between values. For example, the two metadata headers 'x-amz-meta-username: fred' and 'x-amz-meta-username: barney' would be combined into the single header 'x-amz-meta-username: fred,barney'.
// 4  "Unfold" long headers that span multiple lines (as allowed by RFC 2616, section 4.2) by replacing the folding white-space (including new-line) by a single space.
// 5  Trim any white-space around the colon in the header. For example, the header 'x-amz-meta-username: fred,barney' would become 'x-amz-meta-username:fred,barney'
// 6  Finally, append a new-line (U+000A) to each canonicalized header in the resulting list. Construct the CanonicalizedResource element by concatenating all headers in this list into a single string.
// 
// Positional versus Named HTTP Header StringToSign Elements
// 
// The first few header elements of StringToSign (Content-Type, Date, and Content-MD5) are positional in nature. StringToSign does not include the names of these headers, only their values from the request. In contrast, the 'x-amz-' elements are named; Both the header names and the header values appear in StringToSign.
// 
// If a positional header called for in the definition of StringToSign is not present in your request, (Content-Type or Content-MD5, for example, are optional for PUT requests, and meaningless for GET requests), substitute the empty string ("") in for that position.
// Time Stamp Requirement
// 
// A valid time stamp (using either the HTTP Date header or an x-amz-date alternative) is mandatory for authenticated requests. Furthermore, the client time-stamp included with an authenticated request must be within 15 minutes of the Amazon S3 system time when the request is received. If not, the request will fail with the RequestTimeTooSkewed error status code. The intention of these restrictions is to limit the possibility that intercepted requests could be replayed by an adversary. For stronger protection against eavesdropping, use the HTTPS transport for authenticated requests.
// 
// Some HTTP client libraries do not expose the ability to set the Date header for a request. If you have trouble including the value of the 'Date' header in the canonicalized headers, you can set the time-stamp for the request using an 'x-amz-date' header instead. The value of the x-amz-date header must be in one of the RFC 2616 formats (http://www.ietf.org/rfc/rfc2616.txt). When an x-amz-date header is present in a request, the system will ignore any Date header when computing the request signature. Therefore, if you include the x-amz-date header, use the empty string for the Date when constructing the StringToSign. See the next section for an example.
