"""Small no-network unit tests for the exact compiled-source byte verifier."""
import importlib.util
from pathlib import Path
import tarfile
import unittest

PATH = Path(__file__).resolve().parents[1] / "scripts" / "verify-video-reverse-compiled-source.py"
SPEC = importlib.util.spec_from_file_location("video_reverse_compiled_source_audit", PATH)
assert SPEC and SPEC.loader
auditor = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(auditor)


class CompiledSourceAuditTests(unittest.TestCase):
    def member(self, data, kind=tarfile.REGTYPE):
        info = tarfile.TarInfo("source-bundle/builder-v1.10.6/build.sh")
        info.size = len(data)
        info.type = kind
        return info

    def test_git_blob_hash_known_vector(self):
        self.assertEqual(auditor.git_blob_sha(b"hello\n"),
                         "ce013625030ba8dba906f756967f9e9ca394464a")

    def test_regular_original_bytes_accepted(self):
        data = b"exact compiled recipe\n"
        auditor.verify_member(self.member(data), data, auditor.git_blob_sha(data))

    def test_modified_source_rejected(self):
        before = b"exact compiled recipe\n"
        after = b"modified recipe\n"
        with self.assertRaisesRegex(ValueError, "compiled Git commit"):
            auditor.verify_member(self.member(after), after, auditor.git_blob_sha(before))

    def test_truncated_source_rejected(self):
        data = b"exact compiled recipe\n"
        with self.assertRaisesRegex(ValueError, "Truncated"):
            auditor.verify_member(self.member(data), data[:3],
                                  auditor.git_blob_sha(data))

    def test_source_symlink_rejected(self):
        data = b"file contents\n"
        with self.assertRaisesRegex(ValueError, "regular file"):
            auditor.verify_member(self.member(data, tarfile.SYMTYPE),
                                  data, auditor.git_blob_sha(data))

    def test_expected_recipe_inventory(self):
        self.assertEqual(len(auditor.COMPILED_SOURCE_BLOBS), 17)
        self.assertTrue(all(len(sha) == 40 for sha in
                            auditor.COMPILED_SOURCE_BLOBS.values()))
        self.assertEqual(auditor.SOURCE_NAME,
                         "ffmpeg-wasm-sources-v1.10.6.tar.gz")


if __name__ == "__main__":
    unittest.main()
