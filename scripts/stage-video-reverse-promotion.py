#!/usr/bin/env python3
"""Stage *unchanged* verified 1.10.6 video-reverse binaries and source for review.

The input must be the exact Actions review artifact whose digest has been
independently audited. This does NOT publish a release, create a Git tag,
perform a legal review, or pretend that compiled 1.10.6 bytes are 1.10.7.

Example:
  python3 scripts/stage-video-reverse-promotion.py review.zip --output release-candidate
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import zipfile

import importlib.util

# The already-audited verifier has a hyphenated filename. Load that exact
# script instead of copying or weakening its integrity checks here.
verifier_path = Path(__file__).with_name("verify-video-reverse-review-bundle.py")
spec = importlib.util.spec_from_file_location("reverse_review_verifier", verifier_path)
if spec is None or spec.loader is None:
    raise RuntimeError("Could not load exact review verifier")
verifier = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verifier)
audit = verifier.audit
VERSION = verifier.VERSION

# A profile-specific versioned tag avoids colliding with the already-existing
# Builder-wide "v1.10.6" tag and its release assets. This is a publication
# *proposal*, not a tag this script is permitted to create.
PROFILE_TAG = f"video-reverse-v{VERSION}"
BUILDER_SOURCE_COMMIT = "8e60da7cf9de644ce3f6cce9c007b17e6a385c53"
BROWSER_CONSUMER = "ttomohisa/htmlapps-reverse-clip"
BINARY_ASSETS = (
    f"ffmpeg-wasm-video-reverse-single-thread-v{VERSION}.zip",
    f"ffmpeg-wasm-video-reverse-multi-thread-v{VERSION}.zip",
)
SOURCE_ASSET = f"ffmpeg-wasm-sources-v{VERSION}.tar.gz"
INFO_ASSETS = (
    "BUILDINFO-video-reverse-single-thread.txt",
    "BUILDINFO-video-reverse-multi-thread.txt",
)
CHECKSUM_ASSET = "SHA256SUMS.txt"
ASSETS = (*BINARY_ASSETS, SOURCE_ASSET, *INFO_ASSETS)
SUPPLEMENT_NAME = "VIDEO_REVERSE_LICENSE_ADDENDUM.md"
SUPPLEMENT_PATH = Path(__file__).resolve().parents[1] / "docs" / SUPPLEMENT_NAME


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def stage(candidate: Path, target: Path) -> dict:
    # Reject any accidental repack, overwrite or mutation of reviewed bytes.
    # audit() checks outer ZIP SHA, all five checksums, inner JS/WASM/loader
    # hashes, license/notice presence and corresponding-source tree entries.
    audit(candidate)
    if target.exists() and any(target.iterdir()):
        raise ValueError(f"Refusing to overwrite existing candidate folder: {target}")
    target.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(candidate) as original:
        lines = original.read(CHECKSUM_ASSET).decode("ascii").splitlines()
        declared = {}
        for line in lines:
            match = re.fullmatch(r"([a-f0-9]{64})  ([a-zA-Z0-9._-]+)", line)
            if not match:
                raise ValueError("Unexpected checksum line")
            digest, name = match.groups()
            if name in declared:
                raise ValueError("Duplicate checksum entry")
            declared[name] = digest
        if set(declared) != set(ASSETS):
            raise ValueError("Unexpected checksum inventory")
        for name in ASSETS:
            destination = target / name
            # Bound memory even for the ~70MB corresponding-source archive.
            with original.open(name) as src, destination.open("xb") as dst:
                shutil.copyfileobj(src, dst, length=1024 * 1024)
            actual = sha256_file(destination)
            if actual != declared[name]:
                raise ValueError(f"SHA mismatch after staging {name}")
        (target / CHECKSUM_ASSET).write_bytes(original.read(CHECKSUM_ASSET))
    # The original reviewed binary archives are immutable: their embedded
    # notices omit the name "video-reverse". Supplement them as a separately
    # checked, explicit disclosure, never by rewriting the signed-off ZIPs.
    supplement = SUPPLEMENT_PATH.read_bytes()
    if b"GPL-2.0-or-later" not in supplement or b"video-reverse" not in supplement or b"x264" not in supplement:
        raise ValueError("Supplemental GPL/x264 profile notice is incomplete")
    with (target / SUPPLEMENT_NAME).open("xb") as output:
        output.write(supplement)
    result = {
        "schemaVersion": 1,
        "status": "review-only-not-published",
        "publicReleaseApproved": False,
        "gplLegalReviewApproved": False,
        "reproducedCompilation": False,
        "profile": "video-reverse",
        "builderVersion": VERSION,
        "proposedDistinctReleaseTag": PROFILE_TAG,
        "exactCompiledBuilderSourceCommit": BUILDER_SOURCE_COMMIT,
        "consumer": BROWSER_CONSUMER,
        "originalActionsReviewArchiveSha256": sha256_file(candidate),
        "originalActionsReviewArtifactId": 11920042823,
        "sourceArchive": SOURCE_ASSET,
        "additionalNotice": {
            "name": SUPPLEMENT_NAME,
            "sha256": sha256_file(target / SUPPLEMENT_NAME),
            "bytes": (target / SUPPLEMENT_NAME).stat().st_size,
            "purpose": "Clarify video-reverse GPL/x264 and profile tag; does not rewrite audited binary archives",
        },
        "assets": [
            {"name": name, "sha256": declared[name], "bytes": (target / name).stat().st_size}
            for name in ASSETS
        ],
        "publicationConditions": [
            "Owner/legal reviewer confirms corresponding source and notices",
            "Owner authorizes a distinct permanent public versioned release",
            "Release tag must resolve to the exact compiled source commit",
            "Public assets match all SHA-256 values in this manifest",
            "Reverse Clip importer lock is updated only after durable assets exist",
            "Exact deployed ST and isolated MT app pass browser and privacy tests",
        ],
    }
    (target / "VIDEO_REVERSE_PROMOTION_MANIFEST.json").write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"[PASS] Staged {len(ASSETS)} unmodified checked binaries, source and BUILDINFO")
    print(f"[PASS] Separate GPL/x264 video-reverse disclosure {SUPPLEMENT_NAME} ({result['additionalNotice']['sha256']})")
    print(f"[REVIEW ONLY] Proposed separate tag: {PROFILE_TAG} (NOT created)")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("review_zip", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    stage(args.review_zip, args.output)


if __name__ == "__main__":
    main()
