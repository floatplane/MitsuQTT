#!/bin/bash
if [[ -d "$HOME/.platformio/penv/bin" ]]; then
    export PATH=$PATH:$HOME/.platformio/penv/bin
    echo "Added PlatformIO tools to the path"
else
    echo "No PlatformIO installation detected"
fi
