#!/bin/sh
echo ">> prepare to install prerequisite packages..."
sudo apt-get install bison libasound2-dev swig python-dev mplayer flac libsndfile1-dev libflac++-dev

echo ">> install humix-sense node modules"
cd ../sense/
npm install

echo ">> install humix-dialog-module node modules"
cd ../sense/modules/core/humix-dialog-module
npm install
