#!/bin/bash
docker stop multiplexer
docker stop sender1
docker stop sender2
docker stop sender3
docker stop receiver
docker ps -a
docker container prune -f
