
killall gnatsd-arm
killall node

../bin/gnatsd-arm -V &
node app.js
