var LRU = require("./");

var cache = LRU( {
    max: 1,
    maxAge: 1000
} );

cache.set( "1234", 1 );

setTimeout( function() {
    cache.set( "1234", 2 );
    console.log( "testing after 5s: " + cache.get( "1234" ) );
}, 500 );

setTimeout( function() {
    console.log( "testing after 9s: " + cache.get( "1234" ) );
}, 900 );

setTimeout( function() {
    console.log( "testing after 11s: " + cache.get( "1234" ) );
}, 1100 );

setTimeout( function() {
    console.log( "testing after 16s: " + cache.get( "1234" ) );
}, 1600 );
