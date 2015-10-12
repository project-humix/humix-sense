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
    nats = require('nats').connect(),
    WebSocket = require('ws');

var socket = null,
    currentState = 'STOPPED',
    config = {};

function thinkCommandHandler(data, flags) {

    log.debug('Received message: '+data);

    try {
        var message = JSON.parse(data),
            msgHeader = message.header || undefined,
            msgPayload = message.payload || undefined;

        log.info('msgHeader: %s, msgPayload: %s', JSON.stringify(msgHeader), JSON.stringify(msgPayload));
        if (msgHeader && msgPayload) {
            switch (msgHeader.type) {
                case 'modules':
                    // publish command to the specified module
                    var moduleType = msgPayload.type || undefined,
                        moduelCommand = msgPayload.command || undefined;
                    if (moduleType && moduelCommand) {
                        nats.publish(moduleType, moduelCommand);
                    } else {
                        log.error('Malformed module command!');
                    }
                    break;
                default:
                    log.error('Unknown message type received: '+msgHeader);
            }
        } else {
            log.error('Invalid request format');
        }
    } catch (err) {
        log.error('Unexpected error: '+err);
    }
}

function registerModulesToThink(moduleList) {

}

function publishToThink(topic, message) {
    if (socket && socket.readyState === WebSocket.CONNECTING) {
        var event = {
            topic: topic,
            mesasge: message
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
function connectToThink(id) {

    if (!id) {
        log.error('Sense Id is not provided.');
        throw new Error('Sense Id is not provided');
    }

    socket = new WebSocket(config.url);

    function reconnect(id) {
        if (socket) {
            // clean up the old socket if it exists
            socket.destroy();
            socket = null;
        }
        setTimeout(function() {
            connectToThink(id);
        }, config.options.hasOwnProperty('reconnectInterval') ? config.options.reconnectInterval : 3);
    }

    socket.on('open', function () {
        log.info('Connected to Think successfully.');
        currentState = 'RUNNING';
    });

    socket.on('message', thinkCommandHandler);

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
    var url = thinkUrk || undefined,
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
    connectToThink(config.id);
}

function stop() {
    if (socket) {
        socket.close();
        socket.destroy();
    }
}

module.exports = {
    init: init,
    start: start,
    stop: stop,
    publishToThink: publishToThink,
    state: currentState
};
