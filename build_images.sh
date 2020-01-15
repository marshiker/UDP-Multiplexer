#!/bin/bash
docker build -t receiver-1.0 -f Dockerfile-receiver .
docker build -t sender-1.0 -f Dockerfile-sender .
docker build -t multiplexer-1.0 -f Dockerfile-multiplexer .
