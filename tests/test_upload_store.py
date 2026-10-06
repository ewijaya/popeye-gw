"""Offline checks: no login, network traffic, or real store changes."""
import copy
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import zipfile

SPEC = importlib.util.spec_from_file_location("upload_store", Path(__file__).resolve().parents[1] / "tools/upload_store.py")
store = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(store)


class Publisher:
    @classmethod
    def _post_with_wait_bar(cls, url, headers, data, files, timeout, label):
        raise AssertionError("Unguarded transport")

    @classmethod
    def _upload_release(cls, api_base, app_id, firebase_id_token, pbw_path, version,
                        release_notes, is_published, gif_paths, screenshot_paths, replace_screenshots=False):
        with open(pbw_path, 'rb') as handle:
            return cls._post_with_wait_bar(api_base + '/api/dashboard/apps/' + app_id + '/releases',
                {'Authorization': 'Bearer fake'},
                {'version': version, 'releaseNotes': release_notes, 'isPublished': 'true', 'replaceScreenshots': 'false'},
                [('pbwFile', ('popeye-gw.pbw', handle, 'application/octet-stream'))], 300, 'upload')


class Response:
    status_code = 200

    def json(self):
        return {'ok': True}


class UploadTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.folder = Path(self.tmp.name)
        self.config = store.app_config()
        self.notes = 'Approved notes'
        with zipfile.ZipFile(self.folder / 'popeye-gw.pbw', 'w') as archive:
            archive.writestr('appinfo.json', json.dumps({
                'uuid': self.config['uuid'], 'versionLabel': '1.0.2', 'targetPlatforms': ['emery'],
                'watchapp': {'watchface': False}, 'shortName': 'Popeye G&W', 'longName': 'Popeye G&W'}))
        self.pbw = (self.folder / 'popeye-gw.pbw').read_bytes()
        self.manifest = {'version': '1.0.2', 'pbw_sha256': store.digest(self.pbw),
            'pbw_bytes': len(self.pbw), 'notes_sha256': store.digest(self.notes.encode()),
            'store_app_id': self.config['store_app_id'], 'destinations': ['github', 'appstore'],
            'installed_on_pt2_at': 'test', 'owner_playtest': 'test',
            'approval': {'pbw_sha256': store.digest(self.pbw), 'destinations': ['github', 'appstore'],
                         'owner_words': 'fixture approval', 'time': 'test'}}
        (self.folder / 'manifest.json').write_text(json.dumps(self.manifest))
        (self.folder / 'notes.md').write_text(self.notes)
        self.app = {'id': self.config['store_app_id'], 'app_uuid': self.config['uuid'],
            'name': 'Popeye G&W', 'description': 'Keep this', 'visible': True,
            'assets': [{'icon': 'keep.png', 'screenshots': ['one.png'], 'description': 'Keep this'}],
            'releases': [{'version': '1.0.1', 'is_published': True, 'pbw_url': 'old.pbw', 'release_notes': 'Old'}]}
        self.release = {'version': '1.0.2', 'is_published': True, 'pbw_url': 'new.pbw', 'release_notes': self.notes}
        self.posts = 0

    def post(self, url, **kwargs):
        self.assertEqual(url, store.API + '/api/dashboard/apps/' + self.config['store_app_id'] + '/releases')
        self.assertFalse(kwargs['allow_redirects'])
        self.assertEqual(kwargs['files'][0][1][1].read(), self.pbw)
        self.posts += 1
        self.app['releases'].append(copy.deepcopy(self.release))
        return Response()

    def publish(self, post=None):
        with patch.object(store, 'dashboard_app', side_effect=lambda *_: copy.deepcopy(self.app)):
            store.publish(self.folder, self.config, self.manifest, self.notes, None, 'fake',
                          Publisher, post or self.post, lambda _: self.pbw)

    def test_candidate_integrity_and_approval(self):
        store.candidate(self.folder, self.config, True)
        self.manifest.pop('approval')
        (self.folder / 'manifest.json').write_text(json.dumps(self.manifest))
        store.candidate(self.folder, self.config, False)
        with self.assertRaises(store.Stop):
            store.candidate(self.folder, self.config, True)
        (self.folder / 'notes.md').write_text('changed')
        with self.assertRaises(store.Stop):
            store.candidate(self.folder, self.config)

    def test_app_selection_and_candidate_isolation(self):
        game = store.app_config()
        clock = store.app_config('popeye-gw-clock')
        self.assertEqual(game['store_app_id'], 'e9cb2950ca21440798fb1db8')
        self.assertEqual(game['release_dir'], '.release')
        # Registration state is data, not a constant: accept null or a recorded ID.
        self.assertRegex(clock['store_app_id'] or '0' * 24, r'^[0-9a-f]{24}$')
        self.assertEqual(clock['tag_prefix'], 'clock-v')
        for app, folder in ((game, '.release/1.0.2'),
                            (clock, '.release/popeye-gw-clock/1.0.0')):
            self.assertEqual(store.candidate_folder(store.ROOT / folder, app), store.ROOT / folder)
        with self.assertRaises(store.Stop):
            store.app_config('typo')
        with self.assertRaises(store.Stop):
            store.candidate_folder(store.ROOT / '.release/1.0.0', clock)
        with self.assertRaises(store.Stop):
            store.candidate_folder(store.ROOT / '.release/popeye-gw-clock/1.0.0', game)

    def test_clock_identity_registration_and_upload(self):
        self.config = store.app_config('popeye-gw-clock')
        self.config['store_app_id'] = None  # Exercise the unregistered path whatever the config holds.
        self.manifest['app'] = 'popeye-gw-clock'
        pbw_path = self.folder / self.config['artifact_name']
        def write_candidate(watchface=True, uuid=None):
            with zipfile.ZipFile(pbw_path, 'w') as archive:
                archive.writestr('appinfo.json', json.dumps({
                    'uuid': uuid or self.config['uuid'], 'versionLabel': '1.0.2',
                    'targetPlatforms': ['emery'], 'watchapp': {'watchface': watchface},
                    'shortName': self.config['display_name'], 'longName': self.config['display_name']}))
            self.pbw = pbw_path.read_bytes()
            self.manifest.update(pbw_sha256=store.digest(self.pbw), pbw_bytes=len(self.pbw),
                                 store_app_id=self.config['store_app_id'])
            self.manifest['approval']['pbw_sha256'] = store.digest(self.pbw)
            (self.folder / 'manifest.json').write_text(json.dumps(self.manifest))
        write_candidate()
        with self.assertRaisesRegex(store.Stop, 'Existing store App ID'):
            store.candidate(self.folder, self.config)
        self.config['store_app_id'] = '0123456789abcdef01234567'
        write_candidate(False)
        with self.assertRaisesRegex(store.Stop, 'identity mismatch'):
            store.candidate(self.folder, self.config)
        write_candidate(uuid=store.app_config()['uuid'])
        with self.assertRaisesRegex(store.Stop, 'identity mismatch'):
            store.candidate(self.folder, self.config)
        write_candidate()
        store.candidate(self.folder, self.config, True)
        self.app.update(id=self.config['store_app_id'], app_uuid=self.config['uuid'])
        self.publish()
        self.publish()
        self.assertEqual(self.posts, 1)
        self.manifest['app'] = 'popeye-gw'
        (self.folder / 'manifest.json').write_text(json.dumps(self.manifest))
        with self.assertRaisesRegex(store.Stop, 'another app'):
            store.candidate(self.folder, self.config)

    def test_upload_preserves_metadata_and_is_idempotent(self):
        self.publish()
        baseline = (self.folder / 'store-baseline.json').read_bytes()
        self.publish()
        self.assertEqual(self.posts, 1)
        self.assertEqual((self.folder / 'store-baseline.json').read_bytes(), baseline)

    def test_installed_sdk_offline(self):
        try:
            from pebble_tool.commands.publish import PublishCommand
        except ImportError:
            self.skipTest('Use the Pebble Python interpreter for SDK integration')
        with patch.object(store, 'dashboard_app', side_effect=lambda *_: copy.deepcopy(self.app)):
            store.publish(self.folder, self.config, self.manifest, self.notes, None, 'fake',
                          PublishCommand, self.post, lambda _: self.pbw)
        self.assertEqual(self.posts, 1)

    def test_draft_newer_or_conflicting_bytes_block_upload(self):
        for change in ({'is_published': False}, {'version': '1.0.3'}, {'release_notes': 'changed'}):
            with self.subTest(change=change):
                app = copy.deepcopy(self.app)
                app['releases'].append(dict(self.release, **change))
                with self.assertRaises(store.Stop):
                    store.check_existing(app, self.manifest, self.notes, lambda _: self.pbw)
        self.app['releases'].append(self.release)
        with self.assertRaises(store.Stop):
            store.check_existing(self.app, self.manifest, self.notes, lambda _: b'other PBW')

    def test_lost_response_recovers_without_resubmit(self):
        def lost(*args, **kwargs):
            self.post(*args, **kwargs)
            raise TimeoutError()
        with self.assertRaises(TimeoutError):
            self.publish(lost)
        self.publish()
        self.assertEqual(self.posts, 1)

    def test_unknown_outcome_blocks_duplicate(self):
        with self.assertRaises(TimeoutError):
            self.publish(lambda *_args, **_kwargs: (_ for _ in ()).throw(TimeoutError()))
        with self.assertRaises(store.Stop):
            self.publish()
        self.assertEqual(self.posts, 0)

    def test_preservation_failure_keeps_first_baseline(self):
        def changed(*args, **kwargs):
            response = self.post(*args, **kwargs)
            self.app['assets'][0]['icon'] = 'unexpected.png'
            return response
        with self.assertRaises(store.Stop):
            self.publish(changed)
        baseline = json.loads((self.folder / 'store-baseline.json').read_text())
        self.assertEqual(baseline['metadata']['assets'][0]['icon'], 'keep.png')
        with self.assertRaises(store.Stop):
            self.publish()
        self.assertEqual(self.posts, 1)

    def test_changed_or_removed_history_fails(self):
        baseline = {'metadata': store.preserved(self.app), 'history': store.history(copy.deepcopy(self.app))}
        self.app['releases'][0]['release_notes'] = 'changed'
        with self.assertRaises(store.Stop):
            store.check_preserved(self.app, baseline, '1.0.2')
        self.app['releases'] = []
        with self.assertRaises(store.Stop):
            store.check_preserved(self.app, baseline, '1.0.2')

    def test_redirect_is_not_followed(self):
        def redirect(*args, **kwargs):
            self.assertFalse(kwargs['allow_redirects'])
            response = Response()
            response.status_code = 307
            return response
        with self.assertRaises(store.Stop):
            self.publish(redirect)
        with self.assertRaises(store.Stop):
            self.publish()

    def test_changed_sdk_signature_blocks_upload(self):
        class Changed(Publisher):
            @classmethod
            def _upload_release(cls, new_argument):
                pass
        with self.assertRaises(store.Stop):
            store.guarded_store_publisher(Changed, self.config, self.manifest, self.notes, self.post)

    def test_public_pbw_redirects_without_credentials(self):
        try:
            import requests
        except ImportError:
            self.skipTest('Use the Pebble Python interpreter for HTTP transport tests')
        from unittest.mock import MagicMock
        storage = 'https://pebble-appstore-backend.497e529f13ec4afbfce4dfe3cfd3634d.r2.cloudflarestorage.com/file.pbw'
        def response(status, location=None, chunks=()):
            value = MagicMock(status_code=status, headers={'Location': location} if location else {})
            value.__enter__.return_value = value
            value.iter_content.return_value = chunks
            return value
        for location in (storage, 'https://example.com/file.pbw', 'http://assets.repebble.com/file.pbw'):
            with self.subTest(location=location), patch.object(requests, 'get') as get:
                get.side_effect = [response(307, location), response(200, chunks=[self.pbw])]
                if location == storage:
                    self.assertEqual(store.public_pbw('/api/assets/pbw/test'), self.pbw)
                    self.assertEqual(get.call_count, 2)
                else:
                    with self.assertRaises(store.Stop):
                        store.public_pbw('/api/assets/pbw/test')
                    self.assertEqual(get.call_count, 1)
                for call in get.call_args_list:
                    self.assertEqual(call.kwargs, {'timeout': 30, 'allow_redirects': False, 'stream': True})
        with patch.object(requests, 'get', return_value=response(307, storage)) as get:
            with self.assertRaises(store.Stop):
                store.public_pbw('/api/assets/pbw/test')
            self.assertEqual(get.call_count, 4)
        with patch.object(requests, 'get', return_value=response(200, chunks=[b'x' * 2_000_001])):
            with self.assertRaises(store.Stop):
                store.public_pbw('/api/assets/pbw/test')

    def test_transport_rejects_other_url_fields_files_and_bytes(self):
        for field in ('url', 'data', 'files', 'bytes'):
            with self.subTest(field=field):
                guarded = store.guarded_store_publisher(Publisher, self.config, self.manifest, self.notes, self.post)
                args = dict(url=store.API + '/api/dashboard/apps/' + self.config['store_app_id'] + '/releases',
                    headers={}, data={'version': '1.0.2', 'releaseNotes': self.notes,
                    'isPublished': 'true', 'replaceScreenshots': 'false'},
                    files=[('pbwFile', ('f.pbw', io.BytesIO(self.pbw), 'application/octet-stream'))],
                    timeout=60, label='test')
                if field == 'url': args['url'] = 'https://example.com/upload'
                if field == 'data': args['data']['replaceScreenshots'] = 'true'
                if field == 'files': args['files'].append(('icon', ('f', io.BytesIO(b'x'), 'image/png')))
                if field == 'bytes': args['files'][0][1][1].write(b'bad')
                with self.assertRaises(store.Stop):
                    guarded._post_with_wait_bar(**args)
        self.assertEqual(self.posts, 0)


if __name__ == '__main__':
    unittest.main()
