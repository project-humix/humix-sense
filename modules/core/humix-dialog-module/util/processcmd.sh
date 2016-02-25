#!/bin/sh
#*******************************************************************************
# Copyright (c) 2015 IBM Corp.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#******************************************************************************/

filename=$1
outfile=$2
lang=$3
samprate=$4

RESULT=$(flac $filename -f --best --sample-rate $samprate -o $outfile 1>/dev/shm/voice.log 2>/dev/shm/voice.log; curl -s -X POST --data-binary @$outfile --user-agent 'Mozilla/5.0' --header "Content-Type: audio/x-flac; rate=$samprate;" "https://www.google.com/speech-api/v2/recognize?output=json&lang=$lang&key=AIzaSyBOti4mM-6x9WDnZIjIeyEU21OpBXqWBgw&client=Mozilla/5.0" | sed -e 's/[{}]/''/g' | awk -F":" '{print $4}' | awk -F"," '{printf "---=%s=---", $1}' | sed s/---==---//g)
if [ $RESULT ]; then
    echo $RESULT;
    exit 0;
else
    exit 1;
fi
