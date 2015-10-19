var agent = require('./agent'),
    nats = require('nats').connect(),
    fs = require('fs'),
    log = require('logule').init(module, 'App');


var modules = {};

process.on('SIGTERM', function() {
    if (agent.getState() === 'RUNNING') {
        agent.stop();
    }
});

var senseId = process.argv[2] || undefined;
if (!senseId) {
    try {
        senseId = fs.readFileSync('./sense.txt').toString().trim();
    } catch (e) {
        log.error('Error: '+e);
        process.exit(-1);
    }
}

try {

    // TODO : replace with think URL
    agent.init('http://localhost:1880/comms', senseId, {autoreconnect: true});
    agent.start();
} catch (e) {
    log.error('Error: '+e);
}

agent.events.on('module.command', function(data) {

    log.info('Command: '+JSON.stringify(data));

    var module = data.commandType;
    var command = data.commandName;
    var topic = 'humix.sense.'+module+'.command.'+command;

    log.info('topic:'+topic);
    log.info('data:'+data.commandData);

    if(modules.hasOwnProperty(module) &&  modules[module].commands.indexOf(command) != -1 ){

        nats.publish(topic,JSON.stringify(data.commandData));
        log.info(' publish command');

    }else{

        log.info('skip command');
    }

});


// handle module events

nats.subscribe('humix.sense.*.event.*', function(data){

    log.info('receive module event:'+JSON.stringify(data));

    
    
})

// handle module registration
nats.subscribe('humix.sense.mgmt.register', function(request, replyto){
    log.info("Receive registration :"+ request);

    var requestModule = JSON.parse(request);
    
    modules[requestModule.moduleName] = requestModule;
    // TODO : propagate registration information to cloud


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

    
    console.log('current modules:'+JSON.stringify(modules));
    nats.publish(replyto,'got you');

});

/*

// for testing
setInterval(function() {
    if (agent.getState() === 'CONNECTED') {
        agent.publish('temp', JSON.stringify({
            currentTemp: 25
        }));
    }
}, 3000);

*/
