var fs = require('fs');

console.log('hello from test module');

fs.writeFile('output.txt','test string\n',function(err){

	if(err) console.log("error");
});


setInterval(function(){

    console.log('hello from test module ...');
}, 3000);
