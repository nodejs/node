var hmacsign = require('oauth-sign').hmacsign
  , assert = require('assert')
  , qs = require('querystring')
  , request = require('../index')
  ;

function getsignature (r) {
  var sign
  r.headers.Authorization.slice('OAuth '.length).replace(/,\ /g, ',').split(',').forEach(function (v) {
    if (v.slice(0, 'oauth_signature="'.length) === 'oauth_signature="') sign = v.slice('oauth_signature="'.length, -1)
  })
  return decodeURIComponent(sign)
}

// Tests from Twitter documentation https://dev.twitter.com/docs/auth/oauth

var reqsign = hmacsign('POST', 'https://api.twitter.com/oauth/request_token', 
  { oauth_callback: 'http://localhost:3005/the_dance/process_callback?service_provider_id=11'
  , oauth_consumer_key: 'GDdmIQH6jhtmLUypg82g'
  , oauth_nonce: 'QP70eNmVz8jvdPevU3oJD2AfF7R7odC2XJcn4XlZJqk'
  , oauth_signature_method: 'HMAC-SHA1'
  , oauth_timestamp: '1272323042'
  , oauth_version: '1.0'
  }, "MCD8BKwGdgPHvAuvgvz4EQpqDAtx89grbuNMRd7Eh98")

console.log(reqsign)
console.log('8wUi7m5HFQy76nowoCThusfgB+Q=')
assert.equal(reqsign, '8wUi7m5HFQy76nowoCThusfgB+Q=')

var accsign = hmacsign('POST', 'https://api.twitter.com/oauth/access_token',
  { oauth_consumer_key: 'GDdmIQH6jhtmLUypg82g'
  , oauth_nonce: '9zWH6qe0qG7Lc1telCn7FhUbLyVdjEaL3MO5uHxn8'
  , oauth_signature_method: 'HMAC-SHA1'
  , oauth_token: '8ldIZyxQeVrFZXFOZH5tAwj6vzJYuLQpl0WUEYtWc'
  , oauth_timestamp: '1272323047'
  , oauth_verifier: 'pDNg57prOHapMbhv25RNf75lVRd6JDsni1AJJIDYoTY'
  , oauth_version: '1.0'
  }, "MCD8BKwGdgPHvAuvgvz4EQpqDAtx89grbuNMRd7Eh98", "x6qpRnlEmW9JbQn4PQVVeVG8ZLPEx6A0TOebgwcuA")
  
console.log(accsign)
console.log('PUw/dHA4fnlJYM6RhXk5IU/0fCc=')
assert.equal(accsign, 'PUw/dHA4fnlJYM6RhXk5IU/0fCc=')

var upsign = hmacsign('POST', 'http://api.twitter.com/1/statuses/update.json', 
  { oauth_consumer_key: "GDdmIQH6jhtmLUypg82g"
  , oauth_nonce: "oElnnMTQIZvqvlfXM56aBLAf5noGD0AQR3Fmi7Q6Y"
  , oauth_signature_method: "HMAC-SHA1"
  , oauth_token: "819797-Jxq8aYUDRmykzVKrgoLhXSq67TEa5ruc4GJC2rWimw"
  , oauth_timestamp: "1272325550"
  , oauth_version: "1.0"
  , status: 'setting up my twitter 私のさえずりを設定する'
  }, "MCD8BKwGdgPHvAuvgvz4EQpqDAtx89grbuNMRd7Eh98", "J6zix3FfA9LofH0awS24M3HcBYXO5nI1iYe8EfBA")

console.log(upsign)
console.log('yOahq5m0YjDDjfjxHaXEsW9D+X0=')
assert.equal(upsign, 'yOahq5m0YjDDjfjxHaXEsW9D+X0=')


var rsign = request.post(
  { url: 'https://api.twitter.com/oauth/request_token'
  , oauth: 
    { callback: 'http://localhost:3005/the_dance/process_callback?service_provider_id=11'
    , consumer_key: 'GDdmIQH6jhtmLUypg82g'
    , nonce: 'QP70eNmVz8jvdPevU3oJD2AfF7R7odC2XJcn4XlZJqk'
    , timestamp: '1272323042'
    , version: '1.0'
    , consumer_secret: "MCD8BKwGdgPHvAuvgvz4EQpqDAtx89grbuNMRd7Eh98"
    }
  })

setTimeout(function () {
  console.log(getsignature(rsign))
  assert.equal(reqsign, getsignature(rsign))
})

var raccsign = request.post(
  { url: 'https://api.twitter.com/oauth/access_token'
  , oauth:  
    { consumer_key: 'GDdmIQH6jhtmLUypg82g'
    , nonce: '9zWH6qe0qG7Lc1telCn7FhUbLyVdjEaL3MO5uHxn8'
    , signature_method: 'HMAC-SHA1'
    , token: '8ldIZyxQeVrFZXFOZH5tAwj6vzJYuLQpl0WUEYtWc'
    , timestamp: '1272323047'
    , verifier: 'pDNg57prOHapMbhv25RNf75lVRd6JDsni1AJJIDYoTY'
    , version: '1.0'
    , consumer_secret: "MCD8BKwGdgPHvAuvgvz4EQpqDAtx89grbuNMRd7Eh98"
    , token_secret: "x6qpRnlEmW9JbQn4PQVVeVG8ZLPEx6A0TOebgwcuA" 
    }
  })

setTimeout(function () {
  console.log(getsignature(raccsign))
  assert.equal(accsign, getsignature(raccsign))
}, 1) 

var rupsign = request.post(
  { url: 'http://api.twitter.com/1/statuses/update.json' 
  , oauth: 
    { consumer_key: "GDdmIQH6jhtmLUypg82g"
    , nonce: "oElnnMTQIZvqvlfXM56aBLAf5noGD0AQR3Fmi7Q6Y"
    , signature_method: "HMAC-SHA1"
    , token: "819797-Jxq8aYUDRmykzVKrgoLhXSq67TEa5ruc4GJC2rWimw"
    , timestamp: "1272325550"
    , version: "1.0"
    , consumer_secret: "MCD8BKwGdgPHvAuvgvz4EQpqDAtx89grbuNMRd7Eh98"
    , token_secret: "J6zix3FfA9LofH0awS24M3HcBYXO5nI1iYe8EfBA"
    }
  , form: {status: 'setting up my twitter 私のさえずりを設定する'} 
  })
setTimeout(function () {
  console.log(getsignature(rupsign))
  assert.equal(upsign, getsignature(rupsign))
}, 1)




