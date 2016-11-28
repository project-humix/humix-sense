var express = require('express');
var path = require('path');
var favicon = require('serve-favicon');
var logger = require('morgan');
var cookieParser = require('cookie-parser');
var bodyParser = require('body-parser');
var fs = require('fs');
var sense = require('./sense');

var index = require('./routes/index');

var app = express();

// view engine setup
app.set('views', path.join(__dirname, 'views'));
app.set('view engine', 'ejs');

// uncomment after placing your favicon in /public
//app.use(favicon(path.join(__dirname, 'public', 'favicon.ico')));
app.use(logger('dev'));
app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: false }));
app.use(cookieParser());
app.use(express.static(path.join(__dirname, 'public')));


// render index
app.get('/', function(req, res){
	var init = JSON.parse(fs.readFileSync('config.json', 'utf8')).init;

	// determine wheter config already
	// TODO: different view 
	if (init){
		res.render('init.ejs');
	} else {
		res.render('init.ejs');
	}

});

// show config.json
app.get('/config', function(req, res){
	var config = JSON.parse(fs.readFileSync('config.json', 'utf8'));
	res.json(config);
});

// get new congig and start humix sense
app.get('/sense', function(req, res){

	var config = require('./config.json');

	config.init = false;
	config.thinkURL = req.query.thinkURL;
	config.senseId = req.query.senseId;

	fs.writeFile('./config.json', JSON.stringify(config, null, 2), function (err) {
		if (err) return console.log(err);
		console.log("update config: " + JSON.stringify(config));
	});

	
	sense.humixSenseStart();

	res.json({ result: 'ok' });

});

// catch 404 and forward to error handler
app.use(function(req, res, next) {
	var err = new Error('Not Found');
	err.status = 404;
	next(err);
});

// error handler
app.use(function(err, req, res, next) {
	// set locals, only providing error in development
	res.locals.message = err.message;
	res.locals.error = req.app.get('env') === 'development' ? err : {};

	// render the error page
	res.status(err.status || 500);
	res.render('error');
});

module.exports = app;
