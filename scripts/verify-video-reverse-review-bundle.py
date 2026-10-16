#!/usr/bin/env python3
"""Independent offline integrity audit for the pinned v1.10.6 Reverse Clip candidate.

Input: the six-file ZIP from Builder Actions artifact 11920042823.
No network, no subprocess, no files extracted and no publication.
This checks *integrity/source presence*, not reproducible compilation or GPL law.
"""
import hashlib
import io
import json
import re
import sys
import tarfile
import zipfile
from pathlib import Path

OUTER_SHA = "7b3101d3ad67108354de46c158428f6eeff81b995e4d6aff8008bbffcf2307bb"
VERSION = "1.10.6"
PROFILE = "video-reverse"
EXPECTED = {
    "single-thread": {
        "ffmpeg.js": "4a85934acca9afb09ec90be83958002040d100674fee7c7903e5a59ed1d8a74c",
        "ffmpeg.wasm": "b12598c46d9e98f4f46f5c510476261b9d226ae0de01dc8bd8859d9eeacb108b",
    },
    "multi-thread": {
        "ffmpeg.js": "0f41f937b6535d92159bfafbe063735286e8c693475c4ede856f0df7e8168fa4",
        "ffmpeg.wasm": "8a9e45ce28d06d9dfde734d41d9af022d1158798b4967bb1e05ee41c20cd193c",
    },
}
LOADER_SHA = "32edec91e98e93ef10496a4daaed7e42cabf33ab705df4ca287376ab96cf1ce4"
REQUIRED_BINARY = {
    "ffmpeg.js", "ffmpeg.wasm", "browser-ffmpeg.js", "manifest.json",
    "BUILDINFO.txt", "THIRD_PARTY_NOTICES.md",
    "LICENSES/FFmpeg-COPYING.GPLv2", "LICENSES/x264-COPYING",
    "LICENSES/Emscripten-LICENSE",
}
REQUIRED_SOURCES = {
    "source-bundle/builder-v1.10.6/scripts/build-ffmpeg.sh",
    "source-bundle/builder-v1.10.6/build.sh",
    "source-bundle/builder-v1.10.6/versions.env",
    "source-bundle/builder-v1.10.6/runners/video-reverse.c",
    "source-bundle/builder-v1.10.6/runners/video-reverse-join.inc",
    "source-bundle/builder-v1.10.6/runners/video-reverse-pthread-shutdown.post.js",
    "source-bundle/builder-v1.10.6/profiles/video-reverse/ffmpeg.flags",
    "source-bundle/ffmpeg-n9.0.1/COPYING.GPLv2",
    "source-bundle/x264-31e19f92f00c/COPYING",
    "source-bundle/emscripten-6.0.6/LICENSE",
}


def check(condition, reason):
    if not condition:
        raise ValueError(reason)


def digest(stream):
    h = hashlib.sha256()
    for chunk in iter(lambda: stream.read(1024 * 1024), b""):
        h.update(chunk)
    return h.hexdigest()


def audit(path):
    check(path.is_file(), f"Not a file: {path}")
    with path.open("rb") as raw:
        sha = digest(raw)
    check(sha == OUTER_SHA, f"Unrecognized outer ZIP SHA256: {sha}")
    with zipfile.ZipFile(path) as z:
        expected_files = {
            "SHA256SUMS.txt",
            f"ffmpeg-wasm-sources-v{VERSION}.tar.gz",
            *[f"ffmpeg-wasm-video-reverse-{variant}-v{VERSION}.zip" for variant in EXPECTED],
            *[f"BUILDINFO-video-reverse-{variant}.txt" for variant in EXPECTED],
        }
        check(set(z.namelist()) == expected_files and len(z.namelist()) == 6,
              "Outer ZIP does not contain exactly the six reviewed files")
        checksums = z.read("SHA256SUMS.txt").decode("ascii").splitlines()
        declared = {}
        for line in checksums:
            match = re.fullmatch(r"([0-9a-f]{64})  ([A-Za-z0-9._-]+)", line)
            check(match is not None, f"Invalid SHA256SUMS entry: {line!r}")
            h, name = match.groups()
            check(name not in declared, f"Duplicate checksums: {name}")
            declared[name] = h
        check(set(declared) == expected_files - {"SHA256SUMS.txt"},
              "SHA256SUMS contents differ from expected five files")
        for name, expected_sha in declared.items():
            with z.open(name) as f:
                check(digest(f) == expected_sha, f"Outer checksum mismatch: {name}")
            print(f"[OK] outer checksum {name}")
        for variant, files in EXPECTED.items():
            name = f"ffmpeg-wasm-video-reverse-{variant}-v{VERSION}.zip"
            with zipfile.ZipFile(io.BytesIO(z.read(name))) as inner:
                names = set(inner.namelist())
                check(REQUIRED_BINARY <= names, f"Missing license or executable in {variant}")
                for name, sha in {**files, "browser-ffmpeg.js": LOADER_SHA}.items():
                    with inner.open(name) as asset:
                        check(digest(asset) == sha, f"Binary SHA mismatch {variant}/{name}")
                manifest = json.loads(inner.read("manifest.json"))
                runtime = manifest["runtime"]
                mt = variant == "multi-thread"
                check(manifest["builderVersion"] == VERSION and
                      manifest["profile"] == PROFILE and
                      manifest["binaryLicense"] == "GPL-2.0-or-later",
                      f"Profile / version / license differs: {variant}")
                check(runtime["threading"] == variant and
                      runtime["requiresSharedArrayBuffer"] is mt and
                      runtime["requiresCrossOriginIsolation"] is mt and
                      runtime["fileProtocolSingleHtml"] is (not mt),
                      f"Threading or isolation mismatch: {variant}")
                for name, sha in files.items():
                    check(manifest["files"][name]["sha256"] == sha,
                          f"Manifest checksum mismatch {variant}/{name}")
                print(f"[OK] reviewed {variant}: exact JS/WASM+loader, GPL, isolation")
        source_name = f"ffmpeg-wasm-sources-v{VERSION}.tar.gz"
        source_paths = set()
        with z.open(source_name) as compressed:
            with tarfile.open(fileobj=compressed, mode="r|gz") as tar:
                for member in tar:
                    source_paths.add(member.name)
        missing = REQUIRED_SOURCES - source_paths
        check(not missing, f"Corresponding source / recipe entries missing: {sorted(missing)}")
        check(len(source_paths) >= 25000, "Unexpectedly small source archive")
        print(f"[OK] inspected {len(source_paths)} source paths, including FFmpeg/x264/Emscripten and Builder recipe")
    print("PASS — review-candidate integrity/source presence only. NOT a legal or release approval.")


if __name__ == "__main__":
    try:
        check(len(sys.argv) == 2, "Usage: python3 scripts/verify-video-reverse-review-bundle.py <Actions-artifact-ZIP>")
        audit(Path(sys.argv[1]))
    except (ValueError, OSError, zipfile.BadZipFile, tarfile.TarError, KeyError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        raise SystemExit(1) from exc
