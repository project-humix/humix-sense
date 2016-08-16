# Overview

`Humix` is an open source robot connectivity and design framework that make it easy to
bridge cloud API with hardware sensors and devices. Combining with Watson APIs,
the framework help everyone to build their own cloud-brained robot with a few minimal steps.

Humix leverages NodeRed as the flow-editor for designing how the robot behaves. On top of NodeRed,
a few new nodes are added to support Humix’s module programming model, and make it relatively easy for 
the cloud brain to interact with modules deployed on the robot. 


Before you start configuring your humix sense, make sure you have a `humix think` instance running (local or on bluemix). Steps [Here](https://github.com/project-humix/humix-think)


# Setup Humix Sense

We have simplified the steps required to setup Humix Sense by providing a base image. You can download the image here

Image location ：[humix-sense.img](http://119.81.185.45/humix_image/humix-sense-1.0.gz)

To save disk space, the image is compressed. So to copy the image into your SD card ( at least 16G ), run ( you need to replace the path and your sd disk number)

```
    gzip -dc /path/to/image.gz | dd of=/dev/sdx
```

If you are curious whats inside the base humix sense image, here are you go
- build on jessie-lite 
- install dependent packages (bison libasound2-dev swig python-dev mplayer flac libsndfile1-dev libflac++-dev)
- set USB soundcard as default audio device ( see ~/.asoundrc)
- preload humix-sense
- preload humix-dialog-module. Prebuild and put it under humix-sense/modules/core/ ( any modules under this dir will be launched automatically when humix sense starts)


Assuming you have the base image, here are the following steps:




## Config Network

Configure network setting of raspberry pi as you normally do. For more information, check [here](https://www.raspberrypi.org/documentation/configuration/wireless/wireless-cli.md).





## Update humix sense config

The next step is to tell humix sense where the humix think located. Assuming you have deployed humix think at http:/humix-demo.mybluemix.net. 

```
    cd humix-sense/sense
    vi config.js
```

*** example config ***
```
    module.exports = {
        thinkURL : 'http://humix-demo.mybluemix.net/',
        senseId  : 'humix-demo'
    }
```

## Config STT & TTS credentials
Next, you need to provide the credential of the Speech-Recognition and Text-To-Speech. Depending on your language of preference, you can choose default langage as 'en', 'cht' (traditional chinese) or 'chs' (simplified chinese) 
```
    cd ~/humix-sense/sense/modules/core/humix-dialog-module/
    vi config.js
```

After configured the default language, choose the preferred stt and tts engine, and provide associated credentails. Here I use English as default language and use watson stt and tts services. 
```
...
lang: 'en', // 'en', 'cht' or 'chs'
  'stt-engine': 'watson', // 'watson' or 'google',
  'tts-engine': 'watson', // 'watson' or 'itri' or 'iflytek'
  stt: {
    watson: {
      username: '<your_username>',
      passwd: '<your_password>'
    }
  },
  tts: {
    watson: {
      username: '<your_username>',
      passwd: '<your_password>'
    },
  }
...

```

<img border="0" height="280" src="https://4.bp.blogspot.com/-DLIadhPYcgU/Vw95gLGNfkI/AAAAAAAAAC8/gSkUB4RErfASbhQ8Bx1KybxyiaS4EL0tACLcB/s1600/IBM%2BBluemix%2B-watson.png" width="400" /> <br>


> **< Note > get the credential of Watson STT on bluemix <br>
<img border="0" height="280" src="https://1.bp.blogspot.com/-zrmCCnDEXGw/Vw99ew2egWI/AAAAAAAAADg/FCckacR_BfoIIUx4s1qEvPScVAi7IYBLwCLcB/s1600/IBM%2BBluemix%2Bapp2.png" width="400" /> <br>
<img border="0" height="280" src="https://3.bp.blogspot.com/-vWl2kRxMyek/Vw97S0Yis6I/AAAAAAAAADM/Va-5-Jb8OdAsMpiPL26sySTLsXxs-y90ACLcB/s1600/IBM%2BBluemix%2B-environment.png" width="400" /> <br>
<img border="0" height="280" src="https://3.bp.blogspot.com/-BQOqL-H3xNc/Vw9-Cvod2gI/AAAAAAAAADo/cNa0HT6Qp_4ektVlPy3iuxTy3_I43p0XACLcB/s1600/humix-ng-think_pws.png" width="400" /> <br>


## Lauch Humix Sense

To run humix sense, simply run

    cd ~/humix-sense/sense
    npm start

When you see the follow result, then your `humix sense` has been successfully connected to `humix think`

<img border="0" height="280" src="https://1.bp.blogspot.com/--SaSvdNwxAc/VxDCiCZr2YI/AAAAAAAAAEU/qii75kWgaG46QD--q2HGQ-ihNE-v-MefwCLcB/s1600/pi--humix-ng-sense.png" width="400" /> <br>

You can now config the basic flow on `humix think`


# Copyright and License

Copyright 2016 IBM Corp. Under the Apache 2.0 license.
