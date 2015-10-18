/*jslint node: true */
/**
 * Main function of Agent
 *
 * Usage: ZONE=[zoneId/agentId] nodejs app.js
 *
 * Author: Wei-Ting Chou <wtchou1209@gmail.com>
 *
 */
'use strict';

var log = require('logule').init(module, 'Agent'),
    EventEmitter = require('events').EventEmitter,
    emitter = new EventEmitter,
    WebSocket = require('ws');

var socket = null,
    currentState = 'STOPPED',
    config = {};

function commandHandler(data, flags) {

    try {
        var message = JSON.parse(data).data,
            msgHeader = message.header || undefined,
            msgPayload = message.payload || undefined;

        if (msgHeader && msgPayload) {
            switch (msgHeader.type) {
                case 'modules':
                    // publish command to the specified module
                    var type = msgPayload.type || undefined,
                        command = msgPayload.command || undefined;
                    if (type && command) {
                        emitter.emit('moduleCommand', JSON.stringify({type: type, command: command}));
                    } else {
                        log.error('Malformed module command: '+JSON.stringify(message));
                    }
                    break;
                default:
                    log.error('Unknown message type received: '+msgHeader);
            }
        }
    } catch (err) {
        log.error('Unexpected error: '+err);
    }
}

function registerModulesToThink(moduleList) {

}

function publish(topic, message) {
    if (socket && socket.readyState === WebSocket.OPEN) {
        var event = {
            topic: config.id,
            data: {
                topic: topic,
                message: message
            }
        };
        socket.send(JSON.stringify(event), function(error) {
            if (error) {
                log.error('Error occurred while publising event: %s, ERRMSG: %s', JSON.stringify(event), error);
            }
        });
    } else {
        log.error('Connection to Think is broken, message: '+message);
    }
}

/**
 * WebSocket client
 *
 * Remote WebSocket server is the place that provides the dynamic routing data,
 * which URL is given as the third argument 
 *
 */
function connect(id) {

    if (!id) {
        log.error('Sense Id is not provided.');
        throw new Error('Sense Id is not provided');
    }

    socket = new WebSocket(config.url);

    function reconnect(id) {
        // clean up the old socket if any
        destroyConnection();
        setTimeout(function() {
            connect(id);
        }, config.options.hasOwnProperty('reconnectInterval') ? config.options.reconnectInterval : 3000);
    }

    socket.on('open', function () {
        log.info('Connected to Think successfully.');
        currentState = 'CONNECTED';
    });

    socket.on('message', commandHandler);

    socket.on('error', function (err) {
        log.error('Connection to Think is broken! ERRMSG: ' + err);
        if (config.options.hasOwnProperty('autoreconnect') &&
            config.options.autoreconnect === true) {
            reconnect(id);
        }
    });

    socket.on('close', function () {
        log.info('Disconnected to Think!');
        if (config.options.hasOwnProperty('autoreconnect') &&
            config.options.autoreconnect === true) {
            reconnect(id);
        }
    });
}

function init(thinkUrl, senseId, options) {
    var url = thinkUrl || undefined,
        id = senseId || undefined;
    currentState = 'INITIALING';
    if (!url) {
        throw new Error('Url is not provided');
    } else if (!id) {
        throw new Error('Sense Id is not provided');
    }
    config.url = url;
    config.id = id;
    config.options = options || {};

}

function start() {
    currentState = 'CONNECTING';
    connect(config.id);
}

function stop() {
    currentState = 'STOPPED';
    destroyConnection();
}

function destroyConnection() {
    if (socket) {
        socket.close();
        socket = null;
    }
}


module.exports = {
    init: init,
    start: start,
    stop: stop,
    publish: publish,
    events: emitter,
    getState: function () { return currentState; }
};
