var agent = require('./agent'),
    nats = require('nats').connect(),
    fs = require('fs'),
    path = require("path"),
    respawn = require('respawn'),
    bunyan = require("bunyan"),
    log = bunyan.createLogger({name: 'Sense'}),
    config = require('./config');

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
    agent.init(config.thinkURL, senseId, {autoreconnect: true, logger: log});
    agent.start();

} catch (e) {
    log.error('Error: '+e);
}

function humixSenseInit(){
    if (config.log.file) {
      var level = config.log.level || 'info';
      log.addStream({path: config.log.file, level: level});
    }

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
            log.info("module : %s", m);

            var p = respawn(['npm','start'],{cwd:m});

            p.on('stdout', function(data) {
              // sometimes sense module emit multiple JSON logs to this,
              // so need split each JSON and output to file/console
              data.toString().split('\n').forEach(function(e,i,a) {
                try {
                  // quick quess if data is JSON object received from bunyan logging style
                  var m = JSON.parse(e);
                  log.info(m.msg);
                } catch (err) {
                  if (e.trim().length > 0) {
                    log.info(e);
                  }
                }
              });

            });

            p.on('stderr', function(data) {
              data.toString().split('\n').forEach(function(e,i,a) {
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

    log.info('topic: '+topic + ', data: '+JSON.stringify(data.commandData));

    if (modules.hasOwnProperty(module) &&  modules[module].commands.indexOf(command) != -1 ) {
        nats.publish(topic,JSON.stringify(data.commandData));
        log.info('publish command');
    } else {
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

    if (modules.hasOwnProperty(requestModule.moduleName)) {
        log.info('Module [' + requestModule.moduleName + '] already register. Skip');
        nats.publish(replyto,'module already registered');
        return;
    }

    modules[requestModule.moduleName] = requestModule;

    var eventPrefix = 'humix.sense.'+requestModule.moduleName+".event";

    for ( var i in requestModule.events ) {

        var event = requestModule.events[i];
        var module = requestModule.moduleName;
        var topic = eventPrefix + "." + event;

        log.info("subscribing topic: "+ topic);

        (function(topic,module,event){

            nats.subscribe(topic, function(data){
                log.info('about to publish topic: '+topic+', data: %j', data);
                agent.publish(module, event, data);
            });
        })(topic,module,event);

    }

    // register the module to humix-think

    agent.publish('humix-think', 'registerModule', requestModule);

    //console.log('current modules:'+JSON.stringify(modules));
    log.info('current modules: %j', modules);
    nats.publish(replyto,'module registration succeed');

});

//pid.removeOnExit();
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
