import subprocess

build_date = subprocess.check_output(["date", "-u", r"+%Y.%m.%d"]).decode("utf-8").strip()
git_commit = subprocess.check_output(["git", "rev-parse", "--short", "HEAD"]).decode("utf-8").strip()

# Make all available for use in build flags
print("'-DMITSUQTT_BUILD_DATE=\"{0}\"'".format(build_date))
print("'-DMITSUQTT_GIT_COMMIT=\"{0}\"'".format(git_commit))
