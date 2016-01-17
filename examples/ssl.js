
(function(){

    "use strict";

    var fs = require('fs');

    // you'll probably load configuration from config
    var cfg = {
        ssl: true,
        port: 8080,
        ssl_key: '/path/to/you/ssl.key',
        ssl_cert: '/path/to/you/ssl.crt'
    };

    var httpServ = ( cfg.ssl ) ? require('https') : require('http');

    var WebSocketServer   = require('../').Server;

    var app      = null;

    // dummy request processing
    var processRequest = function( req, res ) {

        res.writeHead(200);
        res.end("All glory to WebSockets!\n");
    };

    if ( cfg.ssl ) {

        app = httpServ.createServer({

            // providing server with  SSL key/cert
            key: fs.readFileSync( cfg.ssl_key ),
            cert: fs.readFileSync( cfg.ssl_cert )

        }, processRequest ).listen( cfg.port );

    } else {

        app = httpServ.createServer( processRequest ).listen( cfg.port );
    }

    // passing or reference to web server so WS would knew port and SSL capabilities
    var wss = new WebSocketServer( { server: app } );


    wss.on( 'connection', function ( wsConnect ) {

        wsConnect.on( 'message', function ( message ) {

            console.log( message );

        });

    });


}());