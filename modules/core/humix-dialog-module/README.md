# humix-sense-speech
speech-to-text and text-to-speech enabling project for Humix

# dependencies:
- nodejs >= 4.2
- gcc >= 4.8  
  instructions could be found here https://somewideopenspace.wordpress.com/2014/02/28/gcc-4-8-on-raspberry-pi-wheezy/
- packages
    - sudo apt-get install bison
    - sudo apt-get install libasound2-dev
    - sudo apt-get install swig
    - sudo apt-get install python-dev
    - sudo apt-get install mplayer
    - sudo apt-get install flac
    - sudo apt-get install libsndfile1-dev
    - sudo apt-get install libflac++-dev

- nodes modules:
    - npm install nats soap

- [natsd](https://github.com/nats-io/gnatsd)

# Run the application
Make sure **gnatsd** server is running and you are on the project's root folder. Then issue the following command:
```
    node .
```h
