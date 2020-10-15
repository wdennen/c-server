#!/bin/bash


for j in `seq 1 3`
do
    if [ $j -eq 1 ]
    then
        path="images/test-1kb.img"
        echo "**** small file **** --> path : $path"
    elif [ $j -eq 2 ]
    then
        path="images/test-10mb.img"
        echo "**** medium file **** --> path : $path"
    else
        path="images/test-1gb.img"
        echo "**** large file **** --> path : $path"
    fi

    ./thor.py -t 10 -h 2 http://student04.cse.nd.edu:9894/$path
    echo "./thor.py -t 10 -h 2 http://student04.cse.nd.edu:9894/$path"
    echo
done
