import subprocess

Import("env")

if not env['PIOENV'].startswith("test"):
    # TODO(floatplane) this build date format is duplicated in platformio.ini
    build_date = subprocess.check_output(["date", "-u", r"+%Y.%m.%d"]).decode("utf-8").strip()
    git_commit = subprocess.check_output(["git", "rev-parse", "--short", "HEAD"]).decode("utf-8").strip()
    using_spiffs = 'SPIFFS' in env['PIOENV']
    print('Using SPIFFS: ' + ('YES' if using_spiffs else 'NO'))
    filesystem = '' if using_spiffs else '-LittleFS'

    env.Replace(PROGNAME=("mitsuqtt-%s%s-%s-%s" % (env['PIOENV'], filesystem, build_date, git_commit)))
