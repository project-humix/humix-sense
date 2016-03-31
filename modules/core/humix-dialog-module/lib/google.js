var watson = require('watson-developer-cloud');
var https  = require('https');
var url    = require('url');

var baseURI = 'https://www.google.com/speech-api/full-duplex/v1/';

var kINIT   = 0,
    kREADY  = 1,
    kCLOSED = 2;

function genPairKey() {
	var rev = '';
  	for ( var i = 0; i < 16; i++ ) {
  		rev += Math.floor(Math.random() * 9);
  	}
  	return rev;
}

exports.startSession = function (username, passwd, model, callback) {
    if ( model instanceof Function ) {
        callback = model;
        model = 'zh-tw';
    }
    var pairKey = genPairKey()
    var uriUp = baseURI + 'up?client=chromium' + 
    		'&lang=' + model + '&key=' + passwd +
    		//'&pair=' + pairKey + '&continuous&pFilter=0';
    		'&pair=' + pairKey + '&interim&continuous&pFilter=0';
    var uriDown = baseURI + 'down?pair=' + pairKey;
	
	var partsUp = url.parse(uriUp);
  	var optionsUp = {
    	agent: false,
    	host: partsUp.hostname,
    	port: partsUp.port,
    	path: partsUp.path,
    	method: 'POST',
    	headers: {
      		'Transfer-Encoding': 'chunked',
      		'Content-type': 'audio/l16;rate=16000' } 
      		//'Content-type': 'audio/x-flac;rate=16000' }
  		};

	var partsDown = url.parse(uriDown);
  	var optionsDown = {
    	agent: false,
    	host: partsDown.hostname,
    	port: partsDown.port,
    	path: partsDown.path,
    	method: 'GET'
        }

    var state = kINIT;
    var firstResp = false;
  	var transcribedConn = https.request(optionsDown, function(result) {
    	result.setEncoding('utf-8');
    	result.on('data', function(chunk) {
    	    if ( uploadConn && !firstResp ) {
    	        firstResp = true;
    	        state = kREADY;
    	        uploadConn.emit('ready-to-start');
            }
    	    try {
                var data = JSON.parse(chunk);
                if ( data.result && data.result[0] && data.result[0].final ) {
                    callback(data.result[0].alternative[0].transcript);
                    //uploadConn.emit('next-sentence');
                }
            } catch (e) {
                console.error('got invalid response');
            }
    	});

    	result.on('end', function() {
    	    if ( uploadConn ) {
    	        uploadConn.emit('speech-closed');
            }
            console.error('pulling connection ended');
            state = kCLOSED;
    	});
  	});
  	transcribedConn.end();
  	transcribedConn.on('abort', function() {
        state = kCLOSED;
  	    console.error('pulling connection abort:');
    });
    
    var uploadConn = https.request(optionsUp, function(result) {
    	result.setEncoding('utf-8');
    	var transcript = '';

    	result.on('data', function(chunk) {
    	    try {
                var data = JSON.stringify(chunk);
                if ( data.result && data.result[0] && data.result[0].final ) {
                    callback(data.result[0].alternative[0].transcript);
                }
            } catch (e) {
                console.error('got invalid response');
            }
    	});

    	result.on('end', function() {
    	    console.error('pulling connection closed');
    	    state = kCLOSED;
            uploadConn.emit('speech-closed');
    	});
  	});

    uploadConn.flushHeaders();
    var originalWrite = uploadConn.write;

    uploadConn.write = function (chunk, encoding, callback) {
        if ( state == kCLOSED ) {
            return;
        }
        if ( state == kREADY ) {
            originalWrite.call(uploadConn, chunk, encoding, callback);
        } else {
            uploadConn.once('ready-to-start', function() {
                originalWrite.call(uploadConn, chunk, encoding, callback);
            });
        }
    };

    uploadConn.on('abort', function() {
        state = kCLOSED;
        console.error('upload connection abort');
    });
    
    return uploadConn;
}
