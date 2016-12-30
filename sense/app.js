/* Module dependencies */
var agent = require('./agent'),
    nats = require('nats').connect(),
    fs = require('fs'),
    path = require("path"),
    respawn = require('respawn'),
    log = require('humix-logger').createLogger('Humix-Sense', {filename:'humix-sense.log'}),
    async = require('async'),
    config = require('./config');

var web = require('./web');
var http = require('http');
var fs = require('fs');
var sense = require('./sense');

/* Get port from environment and store in Express. */

var port = normalizePort(process.env.PORT || '3000');
web.set('port', port);

/* Create HTTP server */

var server = http.createServer(web);

/* Listen on provided port, on all network interfaces */

server.listen(port);
server.on('error', onError);
server.on('listening', onListening);

/* Normalize a port into a number, string, or false */

function normalizePort(val) {
	var port = parseInt(val, 10);

	if (isNaN(port)) {
		// named pipe
		return val;
	}

	if (port >= 0) {
		// port number
		return port;
	}
	return false;

}

/* Event listener for HTTP server "error" event */

function onError(error) {
	if (error.syscall !== 'listen') {
		throw error;
	}

	var bind = typeof port === 'string'
		? 'Pipe ' + port
		: 'Port ' + port;

	// handle specific listen errors with friendly messages
	switch (error.code) {
		case 'EACCES':
			log.error(bind + ' requires elevated privileges. Abort');
			process.exit(1);
			break;
		case 'EADDRINUSE':
			log.error(bind + ' is already in use. Abort');
			process.exit(1);
			break;
		default:
			throw error;
	}
}

/* Event listener for HTTP server "listening" event */

function onListening() {
	var addr = server.address();
	var bind = typeof addr === 'string'
		? 'pipe ' + addr
		: 'port ' + addr.port;
	log.info('Listening on ' + bind);
}

log.info('##### Web Server Started #####');

// determine whether config is already set or not
fs.stat('.init', function(err, stat) {
    if(err == null) {
        sense.humixSenseStart();
    }
});