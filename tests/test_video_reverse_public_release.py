"""Offline negative-gate tests for the proposed Reverse Clip public Release auditor.

No GitHub API calls, no Actions artifacts, and no WASM rebuilds.
"""
import importlib.util
from pathlib import Path
import unittest

REPO_ROOT = Path(__file__).resolve().parents[1]
AUDITOR = REPO_ROOT / "scripts" / "verify-video-reverse-public-release.py"
SPEC = importlib.util.spec_from_file_location("reverse_public_release_audit", AUDITOR)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("Cannot load release verifier")
audit = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(audit)


class TestPublicReleaseGate(unittest.TestCase):
    def release(self):
        return {
            "tag_name": audit.TAG,
            "draft": False,
            "prerelease": False,
            "published_at": "2026-10-19T00:00:00Z",
            "html_url": f"https://github.com/{audit.REPO}/releases/tag/{audit.TAG}",
            "assets": [
                {
                    "name": name,
                    "state": "uploaded",
                    "size": 100,
                    "browser_download_url":
                        f"https://github.com/{audit.REPO}/releases/download/{audit.TAG}/{name}",
                }
                for name in sorted(audit.EXPECTED_ASSETS)
            ],
        }

    def test_expected_inventory(self):
        self.assertEqual(len(audit.EXPECTED_ASSETS), 7)
        self.assertEqual(len(audit.check_release_metadata(self.release())), 7)

    def test_draft_rejected(self):
        release = self.release()
        release["draft"] = True
        with self.assertRaises(ValueError):
            audit.check_release_metadata(release)

    def test_prerelease_rejected(self):
        release = self.release()
        release["prerelease"] = True
        with self.assertRaises(ValueError):
            audit.check_release_metadata(release)

    def test_missing_notice_rejected(self):
        release = self.release()
        release["assets"] = [a for a in release["assets"] if
                             a["name"] != "VIDEO_REVERSE_LICENSE_ADDENDUM.md"]
        with self.assertRaises(ValueError):
            audit.check_release_metadata(release)

    def test_duplicate_asset_rejected(self):
        release = self.release()
        release["assets"].append(release["assets"][0])
        with self.assertRaises(ValueError):
            audit.check_release_metadata(release)

    def test_unexpected_asset_rejected(self):
        release = self.release()
        release["assets"].append({
            "name": "unexpected.exe", "state": "uploaded", "size": 42,
            "browser_download_url": "https://example.com/unexpected.exe"})
        with self.assertRaises(ValueError):
            audit.check_release_metadata(release)

    def test_external_asset_url_rejected(self):
        release = self.release()
        release["assets"][0]["browser_download_url"] = "https://example.com/release.zip"
        with self.assertRaises(ValueError):
            audit.check_release_metadata(release)

    def test_moving_tag_rejected(self):
        with self.assertRaisesRegex(ValueError, "wrong Builder source"):
            audit.verify_tag(lambda path: {
                "object": {"type": "commit", "sha": "a" * 40}})

    def test_exact_tag_accepted(self):
        audit.verify_tag(lambda path: {
            "object": {"type": "commit", "sha": audit.SOURCE_COMMIT}})

    def test_annotated_tag_accepted(self):
        def get_json(path):
            if path.startswith("/git/ref/"):
                return {"object": {"type": "tag", "sha": "b" * 40}}
            return {"object": {"type": "commit", "sha": audit.SOURCE_COMMIT}}
        audit.verify_tag(get_json)

    def test_changed_archive_rejected_before_inspection(self):
        bogus_digests = {name: {"sha256": "f" * 64}
                         for name in audit.EXPECTED_ASSETS}
        with self.assertRaisesRegex(ValueError, "SHA-256 mismatch"):
            audit.verify_downloaded(Path("."), bogus_digests)


if __name__ == "__main__":
    unittest.main()
