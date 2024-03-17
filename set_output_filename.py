import subprocess

Import("env")

if not env['PIOENV'].startswith("test"):
    # TODO(floatplane) this is duplicated in platformio.ini
    build_date = subprocess.check_output(["date", "-u", r"+%Y.%m.%d"]).decode("utf-8").strip()
    git_commit = subprocess.check_output(["git", "rev-parse", "--short", "HEAD"]).decode("utf-8").strip()

    env.Replace(PROGNAME=("mitsuqtt-%s-%s-%s" % (env['PIOENV'], build_date, git_commit)))
