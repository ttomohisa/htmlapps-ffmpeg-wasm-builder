#!/usr/bin/env bash
set -euo pipefail
TAG=video-reverse-v1.10.6
COMMIT=8e60da7cf9de644ce3f6cce9c007b17e6a385c53
REPO=ttomohisa/htmlapps-ffmpeg-wasm-builder
[[ "$GITHUB_REPOSITORY" == "$REPO" && "$GITHUB_REF" == "refs/heads/feat/video-reverse-poc" ]] || exit 1
if gh release view "$TAG" >/dev/null 2>&1; then echo 'Already exists; refusing overwrite' >&2; exit 1; fi
# Owner's tag must pre-exist and point exactly at the ORIGINAL compiled source.
# The Actions GITHUB_TOKEN has contents:write, but GitHub rejects creating a
# tag on a historical commit containing workflow file differences (403).
tag_type="$(gh api "repos/$REPO/git/ref/tags/$TAG" --jq '.object.type')"
tag_sha="$(gh api "repos/$REPO/git/ref/tags/$TAG" --jq '.object.sha')"
if [[ "$tag_type" == "tag" ]]; then
  tag_sha="$(gh api "repos/$REPO/git/tags/$tag_sha" --jq '.object.sha')"
elif [[ "$tag_type" != "commit" ]]; then
  echo "Unsupported original Builder source tag type: $tag_type" >&2; exit 1
fi
[[ "$tag_sha" == "$COMMIT" ]] || { echo "Tag $TAG must point to compiled Builder source $COMMIT" >&2; exit 1; }
curl -fLsS --retry 3 -H "Authorization: Bearer $GH_TOKEN" "https://api.github.com/repos/$REPO/actions/artifacts/12145680508/zip" -o /tmp/rc.zip
[[ $(wc -c </tmp/rc.zip) -gt 70000000 ]]
mkdir -p /tmp/rc-stage /tmp/rc-public
unzip -q /tmp/rc.zip -d /tmp/rc-stage
python3 scripts/verify-video-reverse-staged-promotion.py /tmp/rc-stage
python3 scripts/verify-video-reverse-compiled-source.py /tmp/rc-stage
files=(ffmpeg-wasm-video-reverse-single-thread-v1.10.6.zip ffmpeg-wasm-video-reverse-multi-thread-v1.10.6.zip ffmpeg-wasm-sources-v1.10.6.tar.gz BUILDINFO-video-reverse-single-thread.txt BUILDINFO-video-reverse-multi-thread.txt SHA256SUMS.txt VIDEO_REVERSE_LICENSE_ADDENDUM.md)
for f in "${files[@]}"; do cp "/tmp/rc-stage/$f" "/tmp/rc-public/$f"; done
(cd /tmp/rc-public && sha256sum -c SHA256SUMS.txt)
python3 - <<'PY'
import hashlib
from pathlib import Path
root=Path('/tmp/rc-public')
pins={
 'ffmpeg-wasm-video-reverse-single-thread-v1.10.6.zip':'9087bf4d03b29d141e78c93e9de62a237f2d408f24b43caeef26072e6d3b21b0',
 'ffmpeg-wasm-video-reverse-multi-thread-v1.10.6.zip':'b577be491754ecfa96d0b86021184b82b2fc280f8881bd73cb5d52d83914d3bd',
 'ffmpeg-wasm-sources-v1.10.6.tar.gz':'9ceb47a5f46fb3484f8b1f6a8fa37e6d0dc4144aa09d78939ac9d23e8ab5ce92',
 'VIDEO_REVERSE_LICENSE_ADDENDUM.md':'e011d66e3451dbf5ffa5f67f26636749664a69d285a3cfc665a71e36635e15f3'}
assert len(list(root.iterdir()))==7
for name,pin in pins.items(): assert hashlib.sha256((root/name).read_bytes()).hexdigest()==pin, name
print('PASS original asset checksums')
PY
gh release create "$TAG" /tmp/rc-public/* --draft --latest=false --verify-tag --title 'FFmpeg WASM video-reverse ST + MT (Builder v1.10.6)' --notes-file docs/VIDEO_REVERSE_GITHUB_RELEASE_NOTES.md
[[ "$(gh api "repos/$REPO/git/ref/tags/$TAG" --jq .object.sha)" == "$COMMIT" ]] || { echo 'Incorrect source tag' >&2; exit 1; }
gh release view "$TAG" --json assets,isDraft >/tmp/rc-draft.json
python3 - <<'PY'
import json
r=json.load(open('/tmp/rc-draft.json'))
names=[a['name'] for a in r['assets']]
expected={'ffmpeg-wasm-video-reverse-single-thread-v1.10.6.zip','ffmpeg-wasm-video-reverse-multi-thread-v1.10.6.zip','ffmpeg-wasm-sources-v1.10.6.tar.gz','BUILDINFO-video-reverse-single-thread.txt','BUILDINFO-video-reverse-multi-thread.txt','SHA256SUMS.txt','VIDEO_REVERSE_LICENSE_ADDENDUM.md'}
assert r['isDraft'] is True and len(names)==7 and set(names)==expected
PY
mkdir -p /tmp/rc-uploaded
gh release download "$TAG" -D /tmp/rc-uploaded
for f in "${files[@]}"; do cmp "/tmp/rc-public/$f" "/tmp/rc-uploaded/$f"; done
echo 'PASS all seven uploaded draft assets identical'
gh release edit "$TAG" --draft=false --latest=false
gh release view "$TAG" --json isDraft,isPrerelease,tagName --jq 'select(.isDraft == false and .isPrerelease == false and .tagName == "video-reverse-v1.10.6") | .tagName' | grep -Fx "$TAG"
echo "PUBLISHED https://github.com/$REPO/releases/tag/$TAG"
env -u GH_TOKEN -u GITHUB_TOKEN python3 scripts/verify-video-reverse-public-release.py --evidence-json /tmp/rc-public-evidence.json
