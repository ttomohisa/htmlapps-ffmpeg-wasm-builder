# Public release procedure

FFmpeg WASM Builder uses GitHub tags as the release boundary. Do not manually upload generated Wasm from a local machine as the canonical release. The tag workflow rebuilds from the pinned source revisions, runs the browser smoke test, prepares license/source bundles, and creates the GitHub Release.

## First public push

1. Create the repository as public and push the `main` branch.
2. Wait for **Build FFmpeg WASM** to finish successfully.
3. Confirm the job ends with `[OK] Smoke test passed.`
4. Only after `main` is green, create the version tag.

For v1.0.0:

```text
git tag -a v1.0.0 -m "FFmpeg WASM Builder v1.0.0"
git push origin v1.0.0
```

`.github/workflows/release.yml` verifies that the pushed tag exactly matches `BUILDER_VERSION` in `versions.env`. A mismatched tag fails instead of publishing a release.

## Release job

The job performs this sequence:

```text
repository checks
      ↓
Docker FFmpeg/x264 build
      ↓
headless Chrome smoke test
      ↓
fetch exact FFmpeg/x264/Emscripten source revisions
      ↓
prepare binary + corresponding-source bundles
      ↓
SHA-256 checksums
      ↓
GitHub Release
```

The workflow calls `gh release create --verify-tag`, so it publishes only an already-pushed Git tag.

## Updating FFmpeg/Emscripten/x264 later

Change one pin at a time, keep exact refs/commits in `versions.env`, and run `build.bat`. After the real browser smoke test succeeds, increment `BUILDER_VERSION`, commit, push `main`, wait for CI, and then push the matching `v<version>` tag.

Do not reuse or move an already-published release tag.
