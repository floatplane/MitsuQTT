import subprocess

Import("env")

if not env['PIOENV'].startswith("test"):
    # TODO(floatplane) this is duplicated in post_extra_script.py
    build_date = subprocess.check_output(["date", "-u", r"+%Y.%m.%d"]).decode("utf-8").strip()
    git_commit = subprocess.check_output(["git", "rev-parse", "--short", "HEAD"]).decode("utf-8").strip()

    env.Replace(PROGNAME=("mitsubishi2mqtt-%s-%s-%s" % (env['PIOENV'], build_date, git_commit)).replace("_", "-"))
