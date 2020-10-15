#!/bin/bash


for j in `seq 1 3`
do
    if [ $j -eq 1 ]
    then
        path="html/"
        echo "**** directory listing **** --> path : $path"
    elif [ $j -eq 2 ]
    then
        path="images/a.png"
        echo "**** static file **** --> path : $path"
    else
        path="scripts/hello.py?name=world"
        echo "**** cgi script **** --> path : $path"
    fi

    ./thor.py -t 10 -h 4 http://student04.cse.nd.edu:9894/$path
    echo "./thor.py -t 10 -h 4 http://student04.cse.nd.edu:9894/$path"
    echo
done
