#!/usr/bin/env python3
"""Create a non-activatable Reverse Clip Release-lock PROPOSAL from audited files.

Only reads a prior staged review directory. Performs no network request, tag,
release, app-repo modification or GPL sign-off. The proposed lock is intentionally
NOT accepted by the current Reverse Clip importer until separately approved.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import zipfile

HERE = Path(__file__).resolve().parent
VERIFIER = HERE / "verify-video-reverse-review-bundle.py"
spec = importlib.util.spec_from_file_location("reverse_verified", VERIFIER)
if spec is None or spec.loader is None:
    raise RuntimeError("Cannot import reviewed binary definitions")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

SOURCE_COMMIT = "8e60da7cf9de644ce3f6cce9c007b17e6a385c53"
REPOSITORY = "ttomohisa/htmlapps-ffmpeg-wasm-builder"
FEATURES = ["pingpong", "audio-mux"]
VERSION = module.VERSION
TAG = f"video-reverse-v{VERSION}"
NOTICE = "VIDEO_REVERSE_LICENSE_ADDENDUM.md"
PROMOTION = "VIDEO_REVERSE_PROMOTION_MANIFEST.json"


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest_file(path):
    h = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def proposal(staged):
    manifest = json.loads((staged / PROMOTION).read_text(encoding="utf-8"))
    require(manifest.get("schemaVersion") == 1, "Unknown staged schema")
    require(manifest.get("status") == "review-only-not-published", "Wrong stage status")
    for key in ("publicReleaseApproved", "gplLegalReviewApproved", "reproducedCompilation"):
        require(manifest.get(key) is False, f"Review status unexpectedly approved: {key}")
    require(manifest.get("profile") == module.PROFILE, "Wrong profile")
    require(manifest.get("builderVersion") == VERSION, "Wrong version")
    require(manifest.get("proposedDistinctReleaseTag") == TAG, "Wrong proposed tag")
    require(manifest.get("exactCompiledBuilderSourceCommit") == SOURCE_COMMIT,
            "Wrong compiled source provenance")
    require(manifest.get("consumer") == "ttomohisa/htmlapps-reverse-clip",
            "Not intended for Reverse Clip")

    names = (
        f"ffmpeg-wasm-video-reverse-single-thread-v{VERSION}.zip",
        f"ffmpeg-wasm-video-reverse-multi-thread-v{VERSION}.zip",
        f"ffmpeg-wasm-sources-v{VERSION}.tar.gz",
        "BUILDINFO-video-reverse-single-thread.txt",
        "BUILDINFO-video-reverse-multi-thread.txt",
    )
    records = manifest.get("assets")
    require(isinstance(records, list) and len(records) == len(names),
            "Expected exactly five reviewed assets")
    by_name = {x["name"]: x for x in records}
    require(set(by_name) == set(names) and len(by_name) == len(records),
            "Wrong or repeated reviewed payload")
    hashes = {}
    for name in names:
        record = by_name[name]
        path = staged / name
        require(re.fullmatch(r"[0-9a-f]{64}", record.get("sha256", "")),
                f"Invalid digest {name}")
        require(path.is_file() and path.stat().st_size == record["bytes"],
                f"Missing/modified staged file {name}")
        sha = digest_file(path)
        require(sha == record["sha256"], f"Staged SHA mismatch: {name}")
        hashes[name] = sha

    supplemental = manifest.get("additionalNotice")
    require(isinstance(supplemental, dict) and supplemental.get("name") == NOTICE,
            "Profile-specific GPL notice missing")
    supplement_path = staged / NOTICE
    require(supplement_path.is_file() and
            supplement_path.stat().st_size == supplemental["bytes"] and
            digest_file(supplement_path) == supplemental["sha256"],
            "GPL notice digest/size mismatch")
    notice_text = supplement_path.read_text(encoding="utf-8")
    require(all(token in notice_text for token in ("video-reverse", "x264", "GPL-2.0-or-later")),
            "GPL notice content incomplete")

    # Independently compare original internal bytes to reviewed lock pins.
    variants = {}
    for variant, internal_hashes in module.EXPECTED.items():
        name = f"ffmpeg-wasm-video-reverse-{variant}-v{VERSION}.zip"
        with zipfile.ZipFile(staged / name) as archive:
            for inner_name, expected in {
                **internal_hashes, "browser-ffmpeg.js": module.LOADER_SHA
            }.items():
                actual = hashlib.sha256(archive.read(inner_name)).hexdigest()
                require(actual == expected,
                        f"Actual {variant} {inner_name} differs from reviewed WASM")
            meta = json.loads(archive.read("manifest.json"))
            require(meta.get("profile") == module.PROFILE and
                    meta.get("builderVersion") == VERSION and
                    meta.get("binaryLicense") == "GPL-2.0-or-later",
                    f"Bad {variant} bundle metadata")
            require(meta["runtime"]["threading"] == variant,
                    f"Wrong variant threading {variant}")
        variants[variant] = {
            "releaseAsset": name,
            "releaseArchiveSha256": hashes[name],
            "assets": internal_hashes,
        }

    # This is deliberately NOT a valid importable Release lock. A human
    # approval after public release plus an exact downloaded-asset test is
    # required before kind/status may be changed and installed.
    candidate_lock = {
        "schemaVersion": 2,
        "kind": "review-only-proposed-github-release-st-mt",
        "status": "review-only-not-published",
        "builderRepository": REPOSITORY,
        "builderCommit": SOURCE_COMMIT,
        "builderTag": TAG,
        "profile": module.PROFILE,
        "builderVersion": VERSION,
        "wasmLicense": "GPL-2.0-or-later",
        "browserRuntimeSha256": module.LOADER_SHA,
        "correspondingSourceAsset": names[2],
        "correspondingSourceSha256": hashes[names[2]],
        "supplementalNoticeAsset": NOTICE,
        "supplementalNoticeSha256": supplemental["sha256"],
        "variants": variants,
        "features": FEATURES,
    }
    require(candidate_lock["kind"] not in (
        "experimental-dual-github-actions-artifact", "github-release-st-mt"
    ), "Proposal incorrectly activatable")
    return {
        "schemaVersion": 1,
        "purpose": "human-review-only; NEVER install as live lock",
        "releasePublished": False,
        "legalApprovalRecorded": False,
        "sourceBinaryReproductionEstablished": False,
        "originalActionsArtifactId": manifest["originalActionsReviewArtifactId"],
        "proposedLock": candidate_lock,
        "promotionRequirements": [
            "Review GPL corresponding source/notice and build recipe with competent reviewer",
            "Approve profile-scoped tag pointing at exact source commit",
            "Publish all binary/source/BUILDINFO/checksum/notice assets on durable GitHub Release",
            "Verify unauthenticated public URLs and exact hashes on the published assets",
            "Only then set kind=github-release-st-mt, status=release-approved and install lock",
            "Rebuild app from public lock, verify standalone ST/MT browsers and privacy",
        ],
    }


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("stage_dir", type=Path)
    p.add_argument("--output", type=Path, required=True)
    args = p.parse_args()
    require(not args.output.exists(), "Refusing to overwrite existing proposal")
    data = proposal(args.stage_dir)
    args.output.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n",
                           encoding="utf-8")
    print(f"[PASS] Non-activatable Release-lock proposal generated: {args.output}")
    print("[BLOCKED] No release, tag, legal approval, app lock update or publication performed.")


if __name__ == "__main__":
    main()
