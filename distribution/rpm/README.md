# Fedora RPM

Build a release (omit the version for latest):

```sh
./distribution/rpm/build-rpm.sh 2.4.5
```

Local runs use the adjacent spec and metadata; build files and RPMs stay in
that directory. Driver sources come from the release, not your checkout.

Without cloning (review the remote script before running it):

```sh
curl -fsSL https://raw.githubusercontent.com/irql-notlessorequal/intel-vaapi-driver/refs/heads/master/distribution/rpm/build-rpm.sh | bash -s -- 2.4.5
```

This downloads packaging from the selected release tag. Versions `<= 2.4.5` use
upstream `master` instead because their release packaging is missing or broken;
driver sources still come from the selected release.

Builds run in a printed temporary directory. Finished RPMs go into your current
directory; existing files are preserved with a unique suffix on the new RPM.
Temporary files are removed, including on failure. Set `TMPDIR` to use another
disk for the build.

If any build dependency is missing, both modes run `sudo dnf install` with terminal
confirmation. Otherwise no sudo is needed. Installing the finished RPM remains manual.

Future changes on `master` may break packaging compatibility with old releases.
