var agent = require('./agent'),
    nats = require('nats').connect(),
    os = require('os'),
    fs = require('fs'),
    path = require('path'),
    respawn = require('respawn'),
    humixLogger = require('humix-logger'),
    async = require('async'),
    config = require('./config');

var modules = {};
var statusCheckHandle;

var log;
var thinkURL;
var senseId;
/* Constants */

var STATUS_CHECK_INTERVAL = 3000;
var STATUS_CHECK_TIMEOUT  = 5000;


humixSenseInit();

try {
    agent.init(thinkURL, senseId, {autoreconnect: true, logger: log});
    agent.start();

} catch (e) {
    log.error('Error: '+e);
}

function humixSenseInit(){


    var config;
    var defaultConfigPath = path.resolve(os.homedir(), '.humix/config.js');

    if(fs.existsSync(defaultConfigPath)){

        defaultConfigFile = require(defaultConfigPath);

        if(defaultConfigFile && defaultConfigFile['sense']){

            config = defaultConfigFile['sense'];
        }
    }else{

        config = require('./config.js');
    }

    thinkURL = config.thinkURL || process.exit(1);
    senseId = config.senseId || 'humix-sense-default';

    if ( config.log){
        var logfile = config.log.filename || 'humix-sense.log';
        var fileLevel = config.log.fileLevel || 'info';
        var consoleLevel = config.log.consoleLevel || 'debug';

        log = humixLogger.createLogger('Humix-Sense', {filename:logfile,
                                                       fileLevel: fileLevel,
                                                       consoleLevel:consoleLevel});
    }else{

        log = humixLogger.createLogger('Humix-Sense')

    }

    log.info("Init Humix Sense. Using Think URL:"+ thinkURL);

    // starting core modules

    var coreModulePath = path.join(__dirname,'modules/core/');

    if(fs.existsSync(coreModulePath)){
        fs.readdir(coreModulePath,function(err, coreModules){

            if(err){

                log.error('Failed to read core modules. Error:'+err);
                return;
            }

            coreModules.map(function (m) {
                return path.join(coreModulePath, m);

            }).filter(function (m) {
                return fs.statSync(m).isDirectory();

            }).forEach(function (m) {
                log.info("loading module : %s", m);

                var p = respawn(['npm','start'],{cwd:m});

                p.on('stdout', function(data) {
                    // sometimes sense module emit multiple JSON logs to this,
                    // so need split each JSON and output to file/console
                    data.toString().split('\n').forEach(function (e, i, a) {
                            try {
                                // quick quess if data is JSON object received from bunyan logging style
                                var m = JSON.parse(e);
                                log.debug(m.msg);
                            } catch (err) {
                                if (e.trim().length > 0) {
                                    log.error(e);
                                }
                            }
                        });

                });

                p.on('stderr', function (data) {
                    data.toString().split('\n').forEach(function (e, i, a) {
                        try {
                            var m = JSON.parse(e);
                            log.error(m.msg);
                        } catch (err) {
                            if (e.trim().length > 0) {
                                log.error(e);
                            }
                        }
                    });
                });

                p.on('spawn', function () {
                    log.info('process for module ['+m+'] spawned')
                });

                p.on('exit', function (code, signal) {
                    log.error({msg: 'process exited, code: ' + code + ' signal: ' + signal});

                });
                p.start();
            });

        });
    }
    statusCheckHandle = startStatusCheck();
}


function startStatusCheck() {

    return setInterval(function () {

        var moduleStatus = [];

        var myPromise = function (ms, module, callback) {
            return new Promise(function(resolve, reject) {
                callback(resolve, reject);
                setTimeout(function() {
                    reject('Status Check promise timed out after ' + ms + ' ms');
                }, ms);
            });
        }


        async.eachSeries(Object.keys(modules), function (module, cb) {

            log.debug('checking status of ' + module);

            myPromise(STATUS_CHECK_TIMEOUT, module, function (resolve, reject) {
                var t = 'humix.sense.mgmt.' + module + ".ping";
                nats.request(t, null, { 'max': 1 }, function (res) {
                    resolve('success');

                })

            }).then(function (result) {
                   log.debug('connection with module ' + module + " succeed");
                   moduleStatus.push({ moduleId: module, status: 'connected' });
                   cb(null);
            }).catch(function () {
                    log.error('connection with module ' + module + " failed");
                    moduleStatus.push({ moduleId: module, status: 'disconnected' });
                    cb(null);
            })

        }, function (err) {
            agent.publish('humix-think', 'module.status', moduleStatus);
        });

    },STATUS_CHECK_INTERVAL);


}

agent.events.on('module.command', function(data) {

    log.info('Command: '+JSON.stringify(data));

    var module = data.commandType;
    var command = data.commandName;
    var topic = 'humix.sense.'+module+'.command.'+command;

    log.debug('topic: '+topic + ', data: '+JSON.stringify(data.commandData));

    if(modules.hasOwnProperty(module) &&  modules[module].commands.indexOf(command) != -1 ){

        if (data.syncCmdId) {

            // TODO: handle timeout here
            data.commandData.syncCmdId = data.syncCmdId;
            nats.request(topic, JSON.stringify(data.commandData), { 'max': 1 }, function (res) {
                agent.publish_syncResult(data.syncCmdId, res);

            })
        } else {
            nats.publish(topic, JSON.stringify(data.commandData));
        }

        log.debug('publish command:'+command+", module:"+module);

    }else{

        log.info('skip command');
    }
});

// handle module registration
nats.subscribe('humix.sense.mgmt.register', function(request, replyto){

    log.info("Receive registration :"+ request);

    var requestModule = JSON.parse(request);

    if(modules.hasOwnProperty(requestModule.moduleName)){
        log.info('Module [' + requestModule.moduleName + '] already register. Skip');
        nats.publish(replyto,'module already registered');
        return;
    }

    modules[requestModule.moduleName] = requestModule;


    var eventPrefix = 'humix.sense.'+requestModule.moduleName+".event";

    for ( var i in requestModule.events){

        var event = requestModule.events[i];
        var module = requestModule.moduleName;
        var topic = eventPrefix + "." + event;

        log.info("subscribing topic:"+ topic);

        (function(topic,module,event){

            nats.subscribe(topic, function(data){
                log.debug('about to publish topic:'+topic+", data:"+data);
                agent.publish(module, event, data);
            });
        })(topic,module,event);

    }

    // register the module to humix-think

    agent.publish('humix-think', 'registerModule', requestModule);

    log.debug('current modules:'+JSON.stringify(modules));
    nats.publish(replyto,'module registration succeed');

});

process.on('SIGTERM', function() {
    if (agent.getState() === 'RUNNING') {
        agent.stop();
    }
});
