var fs = require('fs');

console.log('hello from test2 module');

fs.writeFile('output.txt','test string\n',function(err){

	if(err) console.log("error");
});


setInterval(function(){

    console.log('hello from test2 module ...');
}, 3000);
