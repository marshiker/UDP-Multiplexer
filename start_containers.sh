#!/bin/bash
docker run --net swargam --ip 192.168.10.12 --name multiplexer -d multiplexer-1.0 192.168.10.12 33676 33776 3 192.168.9.10:37865 192.168.9.11:37654 192.168.10.15:28976 192.168.11.155:48763 1048576
docker run --net swargam --ip 192.168.11.155 --name receiver -d receiver-1.0 192.168.11.155 48763 192.168.10.12:33776 1048576
docker run --net swargam --ip 192.168.9.10 --name sender1 -d sender-1.0 A 10 192.168.9.10 37865 192.168.10.12:33676 53248
docker run --net swargam --ip 192.168.9.11 --name sender2 -d sender-1.0 B 20 192.168.9.11 37654 192.168.10.12:33676 53248
docker run --net swargam --ip 192.168.10.15 --name sender3 -d sender-1.0 C 30 192.168.10.15 28976 192.168.10.12:33676 53248
