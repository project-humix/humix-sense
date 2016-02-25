/*******************************************************************************
* Copyright (c) 2015 IBM Corp.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*    http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/
'use strict';
var console = require('console');
var config  = require('./lib/config');
var sys     = require('util');
var nats    = require('nats').connect();
var exec    = require('child_process').exec;
var execSync = require('child_process').execSync;
var soap    = require('soap');
var crypto  = require('crypto');
var net     = require('net');
var fs      = require('fs');
var Buffer  = require('buffer').Buffer;
var path    = require('path');

var HumixSense = require('node-humix-sense');
var HumixSpeech = require('./lib/HumixSpeech').HumixSpeech;

var voice_path = path.join(__dirname, 'voice');
var url = 'http://tts.itri.org.tw/TTSService/Soap_1_3.php?wsdl';
var kGoogle = 0,
    kWatson = 1;
var engineIndex = {'google': kGoogle, 'watson': kWatson };


var moduleConfig = {
    "moduleName":"humix-dialog",
    "commands" : ["say"],
    "events" : ["speech"],
    "debug": true
}

var humix = new HumixSense(moduleConfig);
var hsm;

humix.on('connection', function(humixSensorModule){

    hsm = humixSensorModule;

    console.log('Communication with humix-sense is now ready.');

    hsm.on('say', function(data){
        console.log('data:'+data);
        text2Speech(data);
    });  // end of say command
});




function convertText(text, hash, callback) {
    var args = {
        accountID: 'richchou',
        password: 'zaq12wsx',
        TTStext: text,
        TTSSpeaker: 'Bruce',
        volume: 50,
        speed: -2,
        outType: 'wav'
    };
    soap.createClient(url, function(err, client) {
        client.ConvertText(args, function(err, result) {
            if (err) {
                console.log('err:', err);
                callback(err, null);
            }
            try {
                var id = result.Result.$value.split('&')[2];
                if (id) {
                    console.log('get id:', id);
                    callback(null, id, hash);
                } else {
                    throw 'failed to convert text!';
                }
            } catch (e) {
                console.log(error);
                callback(error, null);
            }
        });
    });
}

function getConvertStatus(id, callback) {
    var args = {
        accountID: 'richchou',
        password: 'zaq12wsx',
        convertID: id
    };
    soap.createClient(url, function(err, client) {
        console.log('msg_id', id);
        client.GetConvertStatus(args, function(err, result) {
            if (err) {
                console.log('err:', err);
                callback(err, null);
            }
            var downloadUrl = result.Result.$value.split('&')[4];
            if (downloadUrl) {
                //console.log('get download url: '+downloadUrl);
                console.log(id, downloadUrl);
                var wav_file = path.join(voice_path, 'text', id + '.wav');
                execSync('wget '+ downloadUrl + ' -O ' + wav_file, {stdio: [ 'ignore', 'ignore', 'ignore' ]});
                callback(null, id);
            } else {
                var error = 'Still converting! result: '+JSON.stringify(result);
                console.log(error);
                callback(error, null);
            }
        });
    });
}

var retry = 0;
function download (id) {
    retry++;
    console.log(id, 'download' );
    getConvertStatus(id, function(err, result) {
        if (err) 
        { 
            console.log('err:', err); 
            if (retry < 10)
            {
               console.log('retry', retry);
               setTimeout(download, 2000, id);
            }
        }
        else 
        {
           var wav_file = path.join(voice_path,'text', result + '.wav');
           console.log('Play wav file:', wav_file);
           sendAplay2HumixSpeech(wav_file);
        }
    });
}

//start HumixSpeech here
var hs;
var commandRE = /---="(.*)"=---/;

/**
 * callback function that is called when
 * HumixSpeech detect a valid command/sentence
 * @param cmdstr a command/sentence in this format:
 *         '----="command string"=---'
 */
function receiveCommand(cmdstr) {
    cmdstr = cmdstr.trim();
    if ( config.engine ) {
        console.error('command found:', cmdstr);
        
        if(hsm)
            hsm.event("speech", cmdstr);

    } else {
        var match = commandRE.exec(cmdstr);
        if ( match && match.length == 2 ) {
            var cmd = match[1];
            console.error('command found:', cmd);
            try {
                nats.publish('humix.sense.speech.event', cmd);
            } catch ( e ) {
                console.error('can not publish to nats:', e);
            }
            //echo mode
            //text2Speech( '{ "text" : "' + cmd + '" }' );
            if ( hs && cmd.indexOf('聖誕') != -1 && cmd.indexOf('快樂') != -1 ) {
                hs.play('./voice/music/jingle_bells.wav');
            }
        }
    }
}

try {
    hs = new HumixSpeech(config.options);
    var engine = config.engine || 'google';
    hs.engine( config[engine].username, config[engine].passwd,
    		engineIndex[engine], require('./lib/' + engine).startSession);
    hs.start(receiveCommand);
} catch ( error ) {
    console.error(error);
}

/**
 * call the underlying HumixSpeech to play wave file
 * @param file wave file
 */
function sendAplay2HumixSpeech( file ) {
    if( hs ) {
        hs.play(file);
    }
}

var msg = '';
var wavehash = new Object();
/**
 * a simple function to perform the nats subscription
 */
function subscribe(  ) {
    nats.subscribe('humix.sense.speech.command', text2Speech);
}

subscribe();

function text2Speech(msg) {
    console.log('Received a message:', msg);
    var text
    var wav_file = '';
    try {
        text = JSON.parse(msg).text;
    } catch (e) {
        console.error('invalid JSON format');
        return;
    }

    if (!text) {
        return console.error('Missing property: msg.text');
    }
    //for safe
    text = text.trim();
    var hash = crypto.createHash('md5').update(text).digest('hex');
    console.log ('hash value:', hash);
    if (wavehash.hasOwnProperty(hash)) {
        var wav_file = path.join(voice_path,'text', wavehash[hash] + '.wav');
        console.log('Play hash wav file:', wav_file);
        sendAplay2HumixSpeech(wav_file);
    } else {
        console.log('hash not found');
        convertText(text, hash, function(err, id, hashvalue) {
            if (err) {
                console.log(err); 
            } else {
                wavehash[hashvalue] = id;
                retry = 0;
                setTimeout(download, 1000, id);
            }
        });
    }
}

process.stdin.resume();
function cleanup() {
    if (hs) {
        hs.stop();
    }
}
process.on('SIGINT', function() {
    cleanup();
    process.exit(0);
});
process.on('SIGHUP', function() {
    cleanup();
    process.exit(0);
});
process.on('SIGTERM', function() {
    cleanup();
    process.exit(0);
});
process.on('exit', function() {
    cleanup();
});
process.on('error', function() {
    cleanup();
});

process.on('uncaughtException', function(err) {
    if ( err.toString().indexOf('connect ECONNREFUSED') ) {
        console.error('exception,', JSON.stringify(err));
        //cleanup();
        //process.exit(0);
    }
});

//setTimeout(function () {
//    sendAplay2HumixSpeech('voice/interlude/pleasesay1.wav');
    //text2Speech( '{ "text" : "Text to Speech" }' );
//}, 5000);
//setTimeout(function () {
//    sendAplay2HumixSpeech('voice/interlude/pleasesay2.wav');
//}, 8000);
//test code start here 
//function testSendAplay() {
//    console.error('send aplay');
//    sendAplay2HumixSpeech('voice/interlude/what.wav');
//    setTimeout(testSendAplay, 5000);
//}
//setTimeout(testSendAplay, 5000);
