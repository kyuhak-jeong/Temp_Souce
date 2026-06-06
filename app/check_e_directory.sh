#!/bin/bash

FOLDER="/mnt/udisk/VideoFiles/Event/PUA/"
INTERVAL=5

while true; do
    reset  
    echo "=== ($(date '+%Y-%m-%d %H:%M:%S')) ==="   
    echo "-----------------------------------"

    for f in "$FOLDER"/*; do
        if [ -f "$f" ]; then
            size=$(stat -c%s "$f")
            echo "$(basename "$f") [ $size ]"
        fi
    done

    sleep $INTERVAL
done
