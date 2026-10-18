#!/usr/bin/env python3
"""Audit *actual bytes* of Builder build inputs in the reviewed source tar.

Each blob ID was independently read from the Git tree at the EXACT compiled
Builder source commit 8e60da7cf9de644ce3f6cce9c007b17e6a385c53.
This audit is offline. It does NOT reproduce a compilation, certify GPL
completeness, create a Release, or change any previously audited archive.
"""
import argparse
import hashlib
from pathlib import Path
import re
import tarfile

VERSION = "1.10.6"
SOURCE_NAME = f"ffmpeg-wasm-sources-v{VERSION}.tar.gz"
PREFIX = f"source-bundle/builder-v{VERSION}/"

# Git blob SHA-1 values read from the pinned compiled-source commit tree,
# independent of the source TAR's own mutable metadata/checksum manifest.
COMPILED_SOURCE_BLOBS = {
    "build.sh": "84e278f3ea568db0e5d435bde9e9b4aa4a5dd7bc",
    "versions.env": "fa70f7893c9f71a6fd27445f72a7f21cb00bfb45",
    "docker/Dockerfile": "a034c63321f67f16c329c6fed05261ac66f823f6",
    "scripts/docker-common.sh": "e20bf00de45dc8cf8085720df2a1a5537b87ab2b",
    "scripts/build-ffmpeg.sh": "dcf788afc5962d675e1ac62ad9e127d96b18a577",
    "scripts/build-x264.sh": "b028407e270b2715c6fe31ed04b30fc75f342f57",
    "scripts/prepare-release.sh": "79c64023d9b3ce4fd2003d3cfab7c07169e18718",
    "scripts/smoke-test.sh": "5cc3152ff07fb3e78e304cd18d874c3b1b567ff2",
    "scripts/pack-smoke-test.sh": "d90a43bb0da8857e396d45838eac78cb0c2921a4",
    "runtime/browser-ffmpeg.js": "6a390d72c97b868fec5ff6bc23e46090c5d9121b",
    "runners/video-reverse.c": "110914207ce2983b556ddd7b2f6ee45f477717ea",
    "runners/video-reverse-join.inc": "f1fcfa441c7b9c104e2656d96d5f12cc9b8673f9",
    "runners/video-reverse-pthread-shutdown.post.js": "f24876e6a0f7b4c9b410eca9c476cd689d6c0bc2",
    "profiles/video-reverse/profile.env": "8df62dcc1840802a649720870cb708c8efb14ff9",
    "profiles/video-reverse/ffmpeg.flags": "e91d166d559811bcd6fab9b747750c98e1106fcf",
    "profiles/video-reverse/README.md": "9df4ac5588d3f6a24ecef94de861c5aca390cc95",
    "THIRD_PARTY_NOTICES.md": "f1fdc48bf04dc33aa424cb182523d487bcbae9b6",
}


def require(condition, error):
    if not condition:
        raise ValueError(error)


def git_blob_sha(data):
    """Hash a regular source file as Git hashed it in the fixed source commit."""
    return hashlib.sha1(b"blob " + str(len(data)).encode("ascii") + b"\0" + data).hexdigest()


def verify_member(member, data, expected):
    require(member.isfile() and not member.issym() and not member.islnk(),
            f"Builder source must be a regular file: {member.name}")
    require(0 < member.size <= 4 * 1024 * 1024,
            f"Builder source size invalid: {member.name}")
    require(len(data) == member.size, f"Truncated Builder source: {member.name}")
    require(git_blob_sha(data) == expected,
            f"Builder source differs from exact compiled Git commit: {member.name}")


def audit(directory):
    archive_path = Path(directory) / SOURCE_NAME
    require(archive_path.is_file() and not archive_path.is_symlink(),
            f"Matching source archive unavailable: {archive_path}")
    remaining = set(COMPILED_SOURCE_BLOBS)
    checked = {}
    # Stream each tar member; never extract or follow paths or symlinks.
    with tarfile.open(archive_path, "r|gz") as archive:
        for member in archive:
            if not member.name.startswith(PREFIX):
                continue
            relative = member.name[len(PREFIX):]
            if relative not in COMPILED_SOURCE_BLOBS:
                continue
            require(relative in remaining, f"Duplicate Builder source member: {relative}")
            remaining.remove(relative)
            require(member.isfile() and member.size <= 4 * 1024 * 1024,
                    f"Builder source member is linked or oversized: {relative}")
            stream = archive.extractfile(member)
            require(stream is not None, f"Unreadable source member: {relative}")
            with stream:
                data = stream.read(4 * 1024 * 1024 + 1)
            verify_member(member, data, COMPILED_SOURCE_BLOBS[relative])
            checked[relative] = data

    require(not remaining, f"Compiled Builder recipe missing: {sorted(remaining)}")
    versions = checked["versions.env"].decode("utf-8")
    for key, value in {
        "BUILDER_VERSION": VERSION,
        "EMSDK_VERSION": "6.0.6",
        "EMSCRIPTEN_COMMIT": "ce75e06884093bcefb86a6b8fd56a5d62a4cc245",
        "FFMPEG_COMMIT": "bf1b838f2ab88b4f8fd83443325c782ea0e0f7fa",
        "X264_COMMIT": "31e19f92f00c7003fa115047ce50978bc98c3a0d",
    }.items():
        require(re.search(r"^" + re.escape(key) + r"=" + re.escape(value) + r"$",
                          versions, re.MULTILINE) is not None,
                f"Compiled Builder version/dependency pin mismatch: {key}")
    dockerfile = checked["docker/Dockerfile"].decode("utf-8")
    require("FROM emscripten/emsdk:${EMSDK_VERSION} AS toolchain-base" in dockerfile,
            "Unexpected compiled toolchain reference")
    require('RELEASE_PROFILE_SELECTION:-' in checked["scripts/prepare-release.sh"].decode("utf-8"),
            "Missing dedicated source package selector in compiled Builder recipe")
    require("video-reverse" in checked["scripts/build-ffmpeg.sh"].decode("utf-8"),
            "Compiled FFmpeg build script lacks dedicated profile")
    require("single-thread,multi-thread" in
            checked["profiles/video-reverse/profile.env"].decode("utf-8"),
            "Compiled source profile does not contain both threading variants")

    print(f"PASS {len(checked)} exact compiled-commit Builder recipe Git blob IDs")
    print("Source TAR matches original build inputs for checked paths; archived upstream"
          " source presence checked separately.")
    print("REVIEW ONLY: not independent reproduction, full source completeness,"
          " GPL legal approval, public distribution, or G3 acceptance.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("stage_dir", type=Path)
    args = parser.parse_args()
    audit(args.stage_dir)


if __name__ == "__main__":
    main()
