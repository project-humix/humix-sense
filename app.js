var agent = require('./agent'),
    nats = require('nats').connect(),
    fs = require('fs'),
    path = require("path"),
    respawn = require('respawn'),
    log = require('logule').init(module, 'Humix-Sense'),
    config = require('./config')


var modules = {};

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
}

agent.events.on('module.command', function(data) {

    log.info('Command: '+JSON.stringify(data));

    var module = data.commandType;
    var command = data.commandName;
    var topic = 'humix.sense.'+module+'.command.'+command;

    log.info('topic:'+topic);
    log.info('data:'+JSON.stringify(data.commandData));

    if(modules.hasOwnProperty(module) &&  modules[module].commands.indexOf(command) != -1 ){

        nats.publish(topic,JSON.stringify(data.commandData));
        log.info('publish command');

    }else{

        log.info('skip command');
    }

});


// handle module events
/*
nats.subscribe('humix.sense.*.event.*', function(data){

    log.info('receive module event:'+JSON.stringify(data));

})
*/

// handle module registration
nats.subscribe('humix.sense.mgmt.register', function(request, replyto){
    log.info("Receive registration :"+ request);

    var requestModule = JSON.parse(request);

    if(modules.hasOwnProperty(requestModule.moduleName)){
        console.log('Module [' + requestModule.moduleName + '] already register. Skip');
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
                log.info('about to publish topic:'+topic+", data:"+data);
                agent.publish(module, event, data);
            });
        })(topic,module,event);

    }

    // register the module to humix-think

    agent.publish('humix-think', 'registerModule', requestModule);

    console.log('current modules:'+JSON.stringify(modules));
    nats.publish(replyto,'module registration succeed');
    
});


// for testing
/*
setInterval(function() {
    if (agent.getState() === 'CONNECTED') {
        agent.publish('temp', 'currentTemp', 25);
    }
}, 3000);
*/

/*
setTimeout(function() {
    if (agent.getState() === 'CONNECTED') {
        console.log('connected....');
        agent.publish('humix-think', 'registerModule', {
            moduleName: 'neopixel',
            commands: ['feel', 'mode', 'color'],
            events: ['event1']
        });
        agent.publish('humix-think', 'registerModule', {
            moduleName: 'tts',
            commands: ['command1', 'command2'],
            events: ['event1', 'event2']
        });
    }
}, 2000);
*/
