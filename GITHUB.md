# Build RSFX on GitHub (no computer needed)

GitHub Actions compiles the ARM plugin, runs the tests and publishes `RSFX-1.1.0-mpc-armv7.zip` as a Release.

1. On github.com: **+ > New repository** (e.g. `rsfx`), create it.
2. **Add file > Upload files**, upload `RSFX-MPC-source.zip` as-is, commit.
3. **Add file > Create new file**, name it exactly `.github/workflows/build.yml`, paste in the contents of
   `.github/workflows/build.yml` from the zip, commit. The workflow unpacks the source zip itself.
4. Open the **Actions** tab and wait for the "build" run to go green (a few minutes).
5. Open **Releases** on the repo front page and download `RSFX-1.1.0-mpc-armv7.zip`.
6. Install with Termius: see `mpc/INSTALL.md`.

If a run fails, open it and read the failing step. The package step refuses to ship a binary that isn't 32-bit
hard-float ARM, that misses the VST2 entry points, or that needs a newer glibc than the JV-880 package does.
