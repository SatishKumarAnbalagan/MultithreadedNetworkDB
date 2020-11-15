#!/bin/sh
clear

# send quit command after 30 seconds, output FAILED if dbserver
# exits with exit(1). [exit(0) is OK, exit(1) is failure]
#
DEFAULTPORT=5000
PORT=${1:-$DEFAULTPORT}
echo $PORT
(sleep 30; echo quit) | ./dbserver $PORT || echo FAILED&

# give it a second to get up and running.
sleep 1
./dbtest --port=$PORT --set key1 'this is a test'

# give it a second to get up and running.
sleep 1
./dbtest --port=$PORT --get key1

# give it a second to get up and running.
sleep 1
./dbtest --port=$PORT --del key1

# give it a second to get up and running.
sleep 1
./dbtest --port=$PORT --overload

# give it a second to get up and running.
sleep 1
./dbtest --port=$PORT --test

# give it a second to get up and running.
sleep 1
./dbtest --port=$PORT --count 10000 --threads 5
wait
