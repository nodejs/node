var connect = require('connect'),
    fs = require('fs'),
    helpers = require('./helpers'),
    testUncompressed = helpers.testUncompressed,
    testCompressed = helpers.testCompressed,
    testRedirect = helpers.testRedirect,
    testMaxAge = helpers.testMaxAge,
    gzip = require('../index'),
    
    fixturesPath = __dirname + '/fixtures',
    cssBody = fs.readFileSync(fixturesPath + '/style.css', 'utf8'),
    htmlBody = fs.readFileSync(fixturesPath + '/index.html', 'utf8'),
    appBody = '<b>Non-static html</b>',
    cssPath = '/style.css',
    gifPath = '/blank.gif',
    htmlPath = '/',
    matchCss = /text\/css/,
    matchHtml = /text\/html/,
    
    staticDefault = connect.createServer(
      gzip.staticGzip(fixturesPath)
    ),
    staticCss = connect.createServer(
      gzip.staticGzip(fixturesPath, { matchType: /css/ }),
      function(req, res) {
        if (req.url === '/app') {
          res.setHeader('Content-Type', 'text/html; charset=utf-8');
          res.setHeader('Content-Length', appBody.length);
          res.end(appBody);
        }
      }
    ),
    staticMaxAge = connect.createServer(
      gzip.staticGzip(fixturesPath, { maxAge: 1234000 })
    );

module.exports = {
  'staticGzip test uncompressable: no Accept-Encoding': testUncompressed(
    staticCss, cssPath, {}, cssBody, matchCss
  ),
  'staticGzip test uncompressable: does not accept gzip': testUncompressed(
    staticCss, cssPath, { 'Accept-Encoding': 'deflate' }, cssBody, matchCss
  ),
  'staticGzip test uncompressable: unmatched mime type': testUncompressed(
    staticCss, htmlPath, { 'Accept-Encoding': 'gzip' }, htmlBody, matchHtml
  ),
  'staticGzip test uncompressable: non-static request': testUncompressed(
    staticCss, '/app', { 'Accept-Encoding': 'gzip' }, appBody, matchHtml
  ),
  'staticGzip test compressable': testCompressed(
    staticCss, cssPath, { 'Accept-Encoding': 'gzip' }, cssBody, matchCss
  ),
  'staticGzip test compressable: multiple Accept-Encoding types': testCompressed(
    staticCss, cssPath, { 'Accept-Encoding': 'deflate, gzip, sdch' }, cssBody, matchCss
  ),
  
  'staticGzip test uncompressable: default content types': testUncompressed(
    staticDefault, htmlPath, {}, htmlBody, matchHtml
  ),
  'staticGzip test compressable: default content types': testCompressed(
    staticDefault, htmlPath, { 'Accept-Encoding': 'gzip' }, htmlBody, matchHtml
  ),

  // See: http://sebduggan.com/posts/ie6-gzip-bug-solved-using-isapi-rewrite
  'staticGzip test uncompressable: IE6 before XP SP2': testUncompressed(
    staticDefault, htmlPath, { 'Accept-Encoding': 'gzip', 'User-Agent': 'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1)' }, htmlBody, matchHtml
  ),
  'staticGzip test compressable: IE6 after XP SP2': testCompressed(
    staticDefault, htmlPath, { 'Accept-Encoding': 'gzip', 'User-Agent': 'Mozilla/5.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1' }, htmlBody, matchHtml
  ),
  'staticGzip test compressable: IE7': testCompressed(
    staticDefault, htmlPath, { 'Accept-Encoding': 'gzip', 'User-Agent': 'Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 5.1)' }, htmlBody, matchHtml
  ),
  'staticGzip test compressable: Chrome': testCompressed(
    staticDefault, htmlPath, { 'Accept-Encoding': 'gzip', 'User-Agent': 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_7_1) AppleWebKit/535.1 (KHTML, like Gecko) Chrome/14.0.835.186 Safari/535.1' }, htmlBody, matchHtml
  ),

  'staticGzip test compressable: subdirectory': testCompressed(
    staticDefault, '/sub/', { 'Accept-Encoding': 'gzip' }, htmlBody, matchHtml
  ),
  'staticGzip test compressable: subdirectory redirect': testRedirect(
    staticDefault, '/sub', { 'Accept-Encoding': 'gzip' }, '/sub/'
  ),
  'staticGzip test compressable with Accept-Encoding: maxAge': testMaxAge(
    staticMaxAge, cssPath, {'Accept-Encoding': 'gzip'}, 1234000
  ),
  'staticGzip test uncompressable with Accept-Encoding: maxAge': testMaxAge(
    staticMaxAge, gifPath, {'Accept-Encoding': 'gzip'}, 1234000
  ),
  'staticGzip test compressable without Accept-Encoding: maxAge': testMaxAge(
    staticMaxAge, cssPath, {}, 1234000
  ),
  'staticGzip test uncompressable without Accept-Encoding: maxAge': testMaxAge(
    staticMaxAge, gifPath, {}, 1234000
  )
}
