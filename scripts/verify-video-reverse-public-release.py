#!/usr/bin/env python3
"""Read-only, unauthenticated verification of an OWNER-APPROVED public video-reverse Release.

This script cannot publish a Release, create a tag or activate Reverse Clip.
Run only after the proposed profile Release has been independently authorized.
Matches the originally audited Builder 1.10.6 bytes; not a legal GPL opinion.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import tempfile
from urllib.parse import urlparse
from urllib.request import Request, urlopen
import zipfile

HERE = Path(__file__).resolve().parent
REPO = "ttomohisa/htmlapps-ffmpeg-wasm-builder"
TAG = "video-reverse-v1.10.6"
SOURCE_COMMIT = "8e60da7cf9de644ce3f6cce9c007b17e6a385c53"
API = f"https://api.github.com/repos/{REPO}"
MAX_ASSET_BYTES = 250 * 1024 * 1024


def load(name):
    spec = importlib.util.spec_from_file_location(name.replace("-", "_"), HERE / name)
    if spec is None or spec.loader is None:
        raise ValueError(f"Missing verifier dependency: {name}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


original = load("verify-video-reverse-review-bundle.py")
stager = load("stage-video-reverse-promotion.py")
staged = load("verify-video-reverse-staged-promotion.py")
EXPECTED_ASSETS = set(stager.ASSETS) | {stager.CHECKSUM_ASSET, stager.SUPPLEMENT_NAME}


def require(ok, message):
    if not ok:
        raise ValueError(message)


def api_json(path):
    req = Request(API + path, headers={"User-Agent": "video-reverse-public-integrity-check",
                                       "Accept": "application/vnd.github+json"})
    with urlopen(req, timeout=30) as stream:
        require(urlparse(stream.url).netloc == "api.github.com", "Unexpected GitHub API redirect")
        data = stream.read(2 * 1024 * 1024 + 1)
    require(len(data) <= 2 * 1024 * 1024, "Oversized GitHub API response")
    return json.loads(data)


def check_release_metadata(release):
    require(release.get("tag_name") == TAG, "Unexpected Release tag")
    require(release.get("draft") is False and release.get("prerelease") is False,
            "Release is draft/prerelease")
    require(bool(release.get("published_at")), "Release is not publicly published")
    require(release.get("html_url") == f"https://github.com/{REPO}/releases/tag/{TAG}",
            "Unexpected Release identity")
    assets = release.get("assets")
    require(isinstance(assets, list), "Missing Release assets")
    names = [a.get("name") for a in assets]
    require(len(names) == len(set(names)) and set(names) == EXPECTED_ASSETS,
            f"Release assets must be exactly the seven reviewed files, got: {names}")
    for asset in assets:
        name = asset["name"]
        url = f"https://github.com/{REPO}/releases/download/{TAG}/{name}"
        require(asset.get("browser_download_url") == url, f"Unexpected asset URL: {name}")
        require(asset.get("state") == "uploaded" and
                type(asset.get("size")) is int and 0 < asset["size"] <= MAX_ASSET_BYTES,
                f"Missing/oversized Release asset: {name}")
    return {a["name"]: a for a in assets}


def verify_tag(get_json=api_json):
    """Require the profile-specific Git tag to resolve to the compiled source commit."""
    ref = get_json(f"/git/ref/tags/{TAG}")
    obj = ref.get("object", {})
    for _ in range(4):
        if obj.get("type") == "commit":
            require(obj.get("sha") == SOURCE_COMMIT, "Release tag points to wrong Builder source")
            return
        require(obj.get("type") == "tag" and re.fullmatch(r"[a-f0-9]{40}", str(obj.get("sha"))),
                "Malformed or unsupported tag reference")
        obj = get_json(f"/git/tags/{obj['sha']}").get("object", {})
    raise ValueError("Excessive nested annotated tags")


def digest_file(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def download_assets(assets, directory):
    results = {}
    for name in sorted(assets):
        meta = assets[name]
        request = Request(meta["browser_download_url"],
                          headers={"User-Agent": "video-reverse-public-integrity-check"})
        with urlopen(request, timeout=120) as stream:
            # GitHub's unauthenticated Release endpoint redirects to its asset CDN.
            host = urlparse(stream.url).hostname
            require(host in {"github.com", "release-assets.githubusercontent.com",
                             "objects.githubusercontent.com"}, f"Unexpected asset host for {name}")
            h = hashlib.sha256()
            count = 0
            with (directory / name).open("xb") as output:
                while block := stream.read(1024 * 1024):
                    count += len(block)
                    require(count <= meta["size"] and count <= MAX_ASSET_BYTES,
                            f"Oversized public download: {name}")
                    output.write(block)
                    h.update(block)
        require(count == meta["size"], f"Truncated public asset: {name}")
        results[name] = {"bytes": count, "sha256": h.hexdigest(),
                         "url": meta["browser_download_url"]}
    return results


def verify_downloaded(directory, results):
    """Check against independent pins, never against only mutable Release checksums."""
    require(set(results) == EXPECTED_ASSETS, "Incomplete public asset set")
    for name, pin in staged.PINNED_PAYLOADS.items():
        require(results[name]["sha256"] == pin, f"Audited ZIP/source SHA-256 mismatch: {name}")
    require(results[stager.SUPPLEMENT_NAME]["sha256"] == staged.PINNED_ADDENDUM_SHA,
            "GPL/x264 supplemental notice SHA-256 mismatch")
    checksums = (directory / stager.CHECKSUM_ASSET).read_text(encoding="ascii").splitlines()
    declared = {}
    for line in checksums:
        match = re.fullmatch(r"([0-9a-f]{64})  ([A-Za-z0-9._-]+)", line)
        require(match is not None, "Malformed original SHA256SUMS")
        sha, name = match.groups()
        require(name not in declared, "Duplicate original checksum")
        declared[name] = sha
    require(set(declared) == set(stager.ASSETS), "Original checksum inventory differs")
    for name, pin in declared.items():
        require(results[name]["sha256"] == pin, f"Original checksum mismatch: {name}")
    for variant, hashes in original.EXPECTED.items():
        name = f"ffmpeg-wasm-video-reverse-{variant}-v{original.VERSION}.zip"
        with zipfile.ZipFile(directory / name) as archive:
            require(original.REQUIRED_BINARY <= set(archive.namelist()),
                    f"Missing runtime/license contents: {variant}")
            for asset, pin in {**hashes, "browser-ffmpeg.js": original.LOADER_SHA}.items():
                require(hashlib.sha256(archive.read(asset)).hexdigest() == pin,
                        f"Inner runtime SHA mismatch: {variant}/{asset}")
            require(archive.read("BUILDINFO.txt") ==
                    (directory / f"BUILDINFO-video-reverse-{variant}.txt").read_bytes(),
                    f"BUILDINFO differs from audited runtime ZIP: {variant}")
            info = json.loads(archive.read("manifest.json"))
            require(info.get("profile") == "video-reverse" and
                    info.get("binaryLicense") == "GPL-2.0-or-later" and
                    info.get("runtime", {}).get("threading") == variant,
                    f"Wrong public runtime manifest: {variant}")
    notice = (directory / stager.SUPPLEMENT_NAME).read_text(encoding="utf-8")
    require(all(x in notice for x in ("video-reverse", "GPL-2.0-or-later", "x264")),
            "GPL addendum is missing required disclosure")
    return True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evidence-json", type=Path,
                        help="Optional local record of public download URLs, sizes and digests")
    args = parser.parse_args()
    # No Actions token, app lock, tag creation, uploads, or release writes.
    release = api_json(f"/releases/tags/{TAG}")
    assets = check_release_metadata(release)
    verify_tag()
    with tempfile.TemporaryDirectory(prefix="reverse-public-release-audit-") as temp:
        results = download_assets(assets, Path(temp))
        verify_downloaded(Path(temp), results)
    report = {
        "schemaVersion": 1, "status": "public-transport-and-integrity-verified-only",
        "verifiedAtUTC": datetime.now(timezone.utc).isoformat(),
        "repository": REPO, "releaseTag": TAG, "compiledSourceCommit": SOURCE_COMMIT,
        "releaseUrl": release["html_url"], "releaseImmutable": release.get("immutable") is True,
        "assets": results, "legalGPLComplianceApproved": False,
        "reproducibleBuildEstablished": False, "consumerPromoted": False,
    }
    if args.evidence_json:
        require(not args.evidence_json.exists(), "Refusing to overwrite evidence report")
        args.evidence_json.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                                      encoding="utf-8")
    print("PASS unauthenticated public Release transport, exact source tag and ST/MT/source/notice SHA pins")
    if not report["releaseImmutable"]:
        print("NOTE GitHub Release API did not attest immutable publication; retain digest verification and availability review")
    print("NOT a GPL/legal approval, reproduced build, app promotion, or release authorization")


if __name__ == "__main__":
    main()
