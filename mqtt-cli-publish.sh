#!/usr/bin/env bash

HOST=$(avahi-resolve --name picomqtt.local | awk '{ print $2 }')

docker run --add-host picomqtt.local:$HOST -v "$PWD/data/server.crt:/cert/server.crt" --rm hivemq/mqtt-cli pub -h picomqtt.local -s -ws -ws:path mqtt -p 443 -V 3 --cafile /cert/server.crt "$@"
