1. add this config to launch.json if it's not there:
```
        {
            "name": "Debug tests",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceFolder}/.pio/build/test-native/program",
            "args": [],
            "stopAtEntry": false,
            "cwd": "${workspaceFolder}",
            "environment": [],
            "externalConsole": false,
            "MIMode": "lldb"
        },
```
2. run `pio debug -e test-native` to rebuild every time (must be a better way to automate this)