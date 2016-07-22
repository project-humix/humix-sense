var agent = require('./agent'),
    nats = require('nats').connect(),
    fs = require('fs'),
    path = require("path"),
    respawn = require('respawn'),
    log = require('logule').init(module, 'Humix-Sense'),
    async = require('async'),
    config = require('./config')


var modules = {};
var statusCheckHandle;


/* Constants */

var STATUS_CHECK_INTERVAL = 3000;
var STATUS_CHECK_TIMEOUT  = 5000;

process.on('SIGTERM', function() {
    if (agent.getState() === 'RUNNING') {
        agent.stop();
    }
});

var senseId = process.argv[2] || undefined;
if (!senseId) {
    senseId = config.senseId || 'humix';
}

humixSenseInit();

try {
    agent.init(config.thinkURL, senseId, {autoreconnect: true});
    agent.start();

} catch (e) {
    log.error('Error: '+e);
}

function humixSenseInit(){

    log.info("Init Humix Sense");

    // starting core modules

    var coreModulePath = './modules/core/';

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
            console.log("module : %s", m);

            var p = respawn(['npm','start'],{cwd:m});

            p.on('stdout', function(data) {
                log.info('stdout:'+data);
                
            });

            p.on('stderr', function(data) {
                log.info('stderr:'+data);
                
            });

            p.on('spawn', function () {
                log.info('process spawned')
            });

            p.on('exit', function (code, signal) {
                log.error({msg: 'process exited, code: ' + code + ' signal: ' + signal});
                
            });
            p.start();
        });
 
    });


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

            log.info('checking status of ' + module);            

            myPromise(STATUS_CHECK_TIMEOUT, module, function (resolve, reject) { 
                var t = 'humix.sense.mgmt.' + module + ".ping";           
                nats.request(t, null, { 'max': 1 }, function (res) { 
                    resolve('success');
                    
                })

            }).then(function (result) { 
                   log.info('connection with module ' + module + " succeed");                    
                   moduleStatus.push({ moduleId: module, status: 'connected' });
                   cb(null);
            }).catch(function () { 
                    log.info('connection with module ' + module + " failed");    
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

    log.info('topic:'+topic);
    log.info('data:'+JSON.stringify(data.commandData));

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
        
        log.debug('publish command');

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

        log.debug("subscribing topic:"+ topic);

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

