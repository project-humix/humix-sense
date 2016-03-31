/*******************************************************************************
* Copyright (c) 2015 IBM Corp.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*    http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

var nats = require('nats').connect();

// publish events
/*
var speech_command = { "sensor" : "temp",
                       "value" : 20 }
nats.publish("humix.sense.speech.command",JSON.stringify(speech_command));

var speech_command = { "sensor" : "age",
                       "value" : 38 }
nats.publish("humix.sense.speech.command",JSON.stringify(speech_command));
*/
var speech_command = { "text" : "這不是一個測試" };
nats.publish("humix.sense.speech.command",JSON.stringify(speech_command));
