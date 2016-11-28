
killall gnatsd-arm
killall node

../bin/gnatsd-darwin -V &
node app.js
