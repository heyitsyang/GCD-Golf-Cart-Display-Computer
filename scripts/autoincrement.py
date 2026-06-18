import sys, re, datetime

PATH_VERSION = './src/version.h'
PATH_USER_MANUAL = './Documentation/GCS User Manual.md'
MAJOR, MINOR, PATCH, BUILD = 0, 1, 2, 3

# Read
with open(PATH_VERSION, 'r') as reader:
  # Find "MAJOR.MINOR.PATCH+BUILD" from the first line
  line = re.search(r'"([^"]*)"', reader.readline()).group()[1:-1]
  # Extract old values for MAJOR.MINOR.PATCH+BUILD
  versions = re.split('\.|\+', line)
  # Increment value
  versions[BUILD] = int(versions[BUILD]) + 1

  # Write
  with open(PATH_VERSION, 'w') as writer:
    time = datetime.datetime.now()

    datestamp = time.strftime('%Y-%m-%d')
    timestamp = time.strftime('%H:%M')
    version = '%s.%s.%s+%d' % (versions[MAJOR], versions[MINOR], versions[PATCH], versions[BUILD])
    versionFull = version + ' %s %s' % (datestamp, timestamp)

    writer.writelines([
      '#define VERSION "%s"' % version,
      '\n#define VERSION_MAJOR %s' % versions[MAJOR],
      '\n#define VERSION_MINOR %s' % versions[MINOR],
      '\n#define VERSION_PATCH %s' % versions[PATCH],
      '\n#define VERSION_BUILD %s' % versions[BUILD],
      '\n#define VERSION_DATE "%s"' % datestamp,
      '\n#define VERSION_TIME "%s"' % timestamp,
      '\n#define VERSION_FULL "%s"' % versionFull
    ])

    print('Release: ' + version)

  # Update GCS User Manual title with current MAJOR.MINOR.PATCH
  semver = '%s.%s.%s' % (versions[MAJOR], versions[MINOR], versions[PATCH])
  with open(PATH_USER_MANUAL, 'r', encoding='utf-8') as f:
    manual = f.read()
  manual = re.sub(r'^# GCS User Manual.*$', '# GCS User Manual for GCD Ver ' + semver, manual, flags=re.MULTILINE)
  with open(PATH_USER_MANUAL, 'w', encoding='utf-8') as f:
    f.write(manual)