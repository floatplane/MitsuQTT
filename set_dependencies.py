import subprocess

Import("env")

from pathlib import Path
templates = list(Path("src/frontend").rglob("*.mst")) + list(Path("src/frontend").rglob("*.css"))
for t in templates:
    env.Depends("$BUILD_DIR/src/frontend/templates.cpp.o", str(t.absolute()))
