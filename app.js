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

agent.events.on('moduleCommand', function(command) {

    log.info('Command: '+command);

    var cmd = JSON.parse(command);


    var moduleType = cmd.type;
    var mdouleCommand = cmd.command;
    var moduleData = cmd.data;

    nats.publish('humix.sense.neopixel.command.feel',command);
    /*
    if(moduleCommand.type === 'neopixel'){

        
    }*/
    
});



// handle module registration
nats.subscribe('humix.sense.mgmt.register', function(request, replyto){
    log.info("Receive registration :"+ request);
    nats.publish(replyto,'got you');


    modules[request.module] = request;
    // TODO : propagate registration information to cloud
    
});

/*

// for testing
setInterval(function() {
    if (agent.getState() === 'RUNNING') {
        agent.publish('temp', JSON.stringify({
            currentTemp: 25
        }));
    }
}, 3000);

*/
