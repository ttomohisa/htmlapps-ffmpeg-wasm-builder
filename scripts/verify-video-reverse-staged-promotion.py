#!/usr/bin/env python3
"""Offline byte-pinned verifier for a previously staged Reverse Clip review set.

Designed for use AFTER original Actions artifact 11920042823 expires. It
accepts only the same unchanged Builder 1.10.6 binary/source hashes, GPL
addendum, and non-activatable app proposal. Never publishes or approves a
release. No network, Docker, or writes.
"""
import argparse
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import re
import tarfile
import zipfile

HERE = Path(__file__).resolve().parent


def load(name):
    path = HERE / name
    spec = importlib.util.spec_from_file_location(name.replace("-", "_").replace(".", "_"), path)
    if spec is None or spec.loader is None:
        raise ValueError(f"Cannot load {path}")
    obj = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(obj)
    return obj


original = load("verify-video-reverse-review-bundle.py")
stager = load("stage-video-reverse-promotion.py")
proposer = load("propose-video-reverse-app-lock.py")

# Independent, previously audited *whole archive* SHA-256 pins, not the
# mutable manifest contained in the re-staged artifact.
PINNED_PAYLOADS = {
    "ffmpeg-wasm-video-reverse-single-thread-v1.10.6.zip":
        "9087bf4d03b29d141e78c93e9de62a237f2d408f24b43caeef26072e6d3b21b0",
    "ffmpeg-wasm-video-reverse-multi-thread-v1.10.6.zip":
        "b577be491754ecfa96d0b86021184b82b2fc280f8881bd73cb5d52d83914d3bd",
    "ffmpeg-wasm-sources-v1.10.6.tar.gz":
        "9ceb47a5f46fb3484f8b1f6a8fa37e6d0dc4144aa09d78939ac9d23e8ab5ce92",
}
PINNED_ADDENDUM_SHA = "e011d66e3451dbf5ffa5f67f26636749664a69d285a3cfc665a71e36635e15f3"
PINNED_ORIGINAL_OUTER_SHA = original.OUTER_SHA
FILES = set(stager.ASSETS)
OPTIONAL_RESTAGE_METADATA = "REVIEW_RESTAGING_PROVENANCE.json"


def require(condition, reason):
    if not condition:
        raise ValueError(reason)


def sha(path):
    h = hashlib.sha256()
    with path.open("rb") as reader:
        for chunk in iter(lambda: reader.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def verify(directory):
    require(directory.is_dir(), f"Staging folder missing: {directory}")
    stage_file = directory / "VIDEO_REVERSE_PROMOTION_MANIFEST.json"
    info = json.loads(stage_file.read_text(encoding="utf-8"))
    require(info.get("schemaVersion") == 1, "Wrong staging schema")
    require(info.get("status") == "review-only-not-published", "Stage marked as released")
    for key in ("publicReleaseApproved", "gplLegalReviewApproved", "reproducedCompilation"):
        require(info.get(key) is False, f"Improper stage approval flag: {key}")
    require(info.get("exactCompiledBuilderSourceCommit") == stager.BUILDER_SOURCE_COMMIT,
            "Wrong compiled Builder source")
    require(info.get("originalActionsReviewArtifactId") == 11920042823,
            "Wrong original Actions artifact provenance")
    require(info.get("originalActionsReviewArchiveSha256") == PINNED_ORIGINAL_OUTER_SHA,
            "Original audit SHA does not match")
    require(info.get("profile") == "video-reverse" and
            info.get("builderVersion") == original.VERSION and
            info.get("proposedDistinctReleaseTag") == stager.PROFILE_TAG,
            "Wrong Builder profile/version/tag")

    entries = info.get("assets")
    require(isinstance(entries, list) and len(entries) == len(FILES),
            "Expected exactly five original reviewed payloads")
    record = {entry["name"]: entry for entry in entries}
    require(len(record) == len(entries) and set(record) == FILES, "Unknown/duplicate asset")

    lines = (directory / stager.CHECKSUM_ASSET).read_text(encoding="ascii").splitlines()
    checksum = {}
    for line in lines:
        matched = re.fullmatch(r"([0-9a-f]{64})  ([A-Za-z0-9._-]+)", line)
        require(matched is not None, "Malformed original checksum line")
        digest, name = matched.groups()
        require(name not in checksum, "Duplicate original checksum line")
        checksum[name] = digest
    require(set(checksum) == FILES, "Original checksum inventory differs")

    for name in sorted(FILES):
        path = directory / name
        require(path.is_file() and path.stat().st_size == record[name].get("bytes"),
                f"Missing/truncated staged artifact: {name}")
        actual = sha(path)
        require(actual == record[name].get("sha256") and actual == checksum[name],
                f"Staged file/checksum discrepancy: {name}")
        if name in PINNED_PAYLOADS:
            require(actual == PINNED_PAYLOADS[name], f"Original audited bytes changed: {name}")
        print(f"[OK] audited staged {name}: {actual[:12]}")

    # BUILDINFO bytes inside the independently pinned zip must be identical
    # to the separately staged BUILDINFO file. Verify original core and
    # browser wrapper SHA pins again, not just the ZIP's packaging SHA.
    for variant, pins in original.EXPECTED.items():
        name = f"ffmpeg-wasm-video-reverse-{variant}-v{original.VERSION}.zip"
        with zipfile.ZipFile(directory / name) as binary:
            require(original.REQUIRED_BINARY <= set(binary.namelist()),
                    f"Original {variant} required notice or assets missing")
            for member, pinned in {**pins, "browser-ffmpeg.js": original.LOADER_SHA}.items():
                require(hashlib.sha256(binary.read(member)).hexdigest() == pinned,
                        f"Original {variant}/{member} SHA differs")
            expected_info = (directory / f"BUILDINFO-video-reverse-{variant}.txt").read_bytes()
            require(binary.read("BUILDINFO.txt") == expected_info,
                    f"Original {variant} staged BUILDINFO differs from binary")
            meta = json.loads(binary.read("manifest.json"))
            require(meta.get("profile") == "video-reverse" and
                    meta.get("builderVersion") == original.VERSION and
                    meta.get("binaryLicense") == "GPL-2.0-or-later" and
                    meta.get("runtime", {}).get("threading") == variant,
                    f"Original {variant} manifest mismatch")

    # Efficient streaming listing; never extract original/source bytes.
    with tarfile.open(directory / stager.SOURCE_ASSET, mode="r:gz") as tar:
        members = {x.name for x in tar}
    require(len(members) >= 25000 and original.REQUIRED_SOURCES <= members,
            "Missing exact corresponding source/build recipe members")
    print(f"[OK] source listing {len(members)} members and pinned archive SHA")

    addendum = info.get("additionalNotice")
    require(isinstance(addendum, dict) and addendum.get("name") == stager.SUPPLEMENT_NAME,
            "Missing supplemental GPL disclosure")
    add_file = directory / stager.SUPPLEMENT_NAME
    require(add_file.is_file() and add_file.stat().st_size == addendum.get("bytes"),
            "GPL disclosure size mismatch")
    require(sha(add_file) == addendum.get("sha256") == PINNED_ADDENDUM_SHA,
            "GPL disclosure deviated from independently audited notice")
    text = add_file.read_text(encoding="utf-8")
    require(all(item in text for item in ("video-reverse", "GPL-2.0-or-later", "x264")),
            "GPL disclosure contents incomplete")

    proposal_file = directory / "REVERSE_CLIP_RELEASE_LOCK_PROPOSAL.json"
    require(proposal_file.is_file(), "Staged proposed app lock missing")
    proposal = json.loads(proposal_file.read_text(encoding="utf-8"))
    require(proposal == proposer.proposal(directory),
            "Stored app proposal no longer matches independently re-computed pins")
    lock = proposal["proposedLock"]
    require(lock["kind"] == "review-only-proposed-github-release-st-mt" and
            lock["status"] == "review-only-not-published" and
            proposal.get("releasePublished") is False and
            proposal.get("legalApprovalRecorded") is False,
            "App lock is inappropriately activatable")

    # Optional provenance from a previous re-stage is *extra* evidence, never
    # a replacement for the immutable pins checked above.
    if (directory / OPTIONAL_RESTAGE_METADATA).is_file():
        restage = json.loads((directory / OPTIONAL_RESTAGE_METADATA).read_text())
        source_id = restage.get("sourceStageArtifactId")
        require(restage.get("schemaVersion") == 1 and
                restage.get("status") == "review-only-not-published" and
                type(source_id) is int and source_id > 0 and
                restage.get("originalBuilderSourceCommit") == stager.BUILDER_SOURCE_COMMIT and
                restage.get("tagPublished") is False and
                restage.get("artifactRetainedForReviewOnly") is True,
                "Unexpected restaging provenance")
    print("PASS staged bytes, corresponding sources, GPL addendum and unapproved app lock")
    print("REVIEW ONLY: no tag, Release, GPL legal signoff or official app lock change")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("stage_dir", type=Path)
    args = p.parse_args()
    verify(args.stage_dir)


if __name__ == "__main__":
    main()
