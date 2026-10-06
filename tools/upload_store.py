#!/usr/bin/env python3
"""Upload an approved frozen PBW to the existing listing; never build or register.

Run with the Python interpreter from the installed pebble executable's shebang.
Default: local candidate checks only. --inspect reads the authenticated listing;
--publish permits one release POST after checking recorded owner approval.
"""
import argparse
import copy
import hashlib
import inspect
import json
from pathlib import Path
import re
import sys
from urllib.parse import urljoin, urlsplit
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]
API = "https://appstore-api.repebble.com"
DASHBOARD = "https://developer.repebble.com"


class Stop(RuntimeError):
    pass


def require(condition, message):
    if not condition:
        raise Stop(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def semver(value):
    require(isinstance(value, str) and re.fullmatch(r"\d+\.\d+\.\d+", value),
            "Unknown release version; inspect before continuing")
    return tuple(map(int, value.split('.')))


def candidate(folder, config, publish=False):
    manifest = json.loads((folder / "manifest.json").read_text())
    pbw = (folder / "popeye-gw.pbw").read_bytes()
    notes = (folder / "notes.md").read_bytes()
    semver(manifest["version"])
    require(digest(pbw) == manifest["pbw_sha256"] and len(pbw) == manifest["pbw_bytes"],
            "Frozen PBW differs from manifest")
    require(digest(notes) == manifest["notes_sha256"], "Frozen notes differ from manifest")
    require(config["appstore_api"] == API and config["dashboard"] == DASHBOARD,
            "Unexpected store hosts")
    require(re.fullmatch(r"[a-f0-9]{24}", config.get("store_app_id") or ""),
            "Existing store App ID required")
    require(manifest.get("store_app_id") == config["store_app_id"],
            "Freeze the destination App ID in the manifest")
    with zipfile.ZipFile(folder / "popeye-gw.pbw") as archive:
        info = json.loads(archive.read("appinfo.json"))
    require(info["uuid"] == config["uuid"] and info["versionLabel"] == manifest["version"]
            and info["targetPlatforms"] == config["platforms"]
            and info["watchapp"].get("watchface") is False
            and info["shortName"] == info["longName"] == config["display_name"],
            "Frozen PBW identity mismatch")
    if publish:
        approval = manifest.get("approval") or {}
        require(approval.get("pbw_sha256") == manifest["pbw_sha256"]
                and set(approval.get("destinations", [])) == set(manifest["destinations"])
                and "appstore" in manifest["destinations"]
                and approval.get("owner_words") and approval.get("time")
                and manifest.get("installed_on_pt2_at") and manifest.get("owner_playtest"),
                "Recorded owner approval and PT2 play-test of this candidate are required")
    return manifest, notes.decode("utf-8")


def response_ok(response):
    require(200 <= response.status_code < 300,
            "Store request failed or redirected (HTTP %s); inspect before retry" % response.status_code)


def dashboard_session():
    import requests
    from pebble_tool.account import get_account
    original = requests.Session.request

    def auth_request(session, method, url, **kwargs):
        parsed = urlsplit(url)
        require(parsed.scheme == "https" and parsed.netloc in {
            "developer.repebble.com", "appstore-api.repebble.com",
            "securetoken.googleapis.com", "identitytoolkit.googleapis.com", "cloud.repebble.com"
        }, "Unexpected authentication host")
        kwargs["allow_redirects"] = False
        response = original(session, method, url, **kwargs)
        response_ok(response)
        return response

    # Cover SDK token refresh as well as its optional account lookup.
    with patch.object(requests.Session, "request", auth_request):
        account = get_account(auth_provider="firebase")
        require(account.is_logged_in, "Run pebble login with the owner's existing account")
        token = account.get_access_token()
    session = requests.Session()
    response_ok(session.post(DASHBOARD + "/api/auth/firebase/session",
        json={"idToken": token}, timeout=30, allow_redirects=False))
    return session, token


def dashboard_app(session, config):
    response = session.get(DASHBOARD + "/api/dashboard/apps/" + config["store_app_id"],
                           timeout=30, allow_redirects=False)
    response_ok(response)
    app = response.json()["app"]
    require(app.get("id") == config["store_app_id"] and app.get("app_uuid") == config["uuid"],
            "Dashboard App ID/UUID mismatch")
    require(isinstance(app.get("releases"), list), "Missing release history")
    return app


def preserved(app):
    result = copy.deepcopy({k: v for k, v in app.items()
                            if k not in {"releases", "latest_release", "updated_at", "updatedAt"}})
    for asset in result.get("assets", []):
        asset.pop("updated_at", None)
        asset.pop("updatedAt", None)
    return result


def history(app):
    result = {}
    for release in app["releases"]:
        version = release["version"]
        semver(version)
        require(version not in result, "Duplicate store version")
        result[version] = release
    return result


def check_preserved(app, baseline, version):
    require(preserved(app) == baseline["metadata"], "Listing metadata/artwork changed; review baseline")
    releases = history(app)
    require(all(releases.get(v) == r for v, r in baseline["history"].items()),
            "A previous release changed or disappeared")
    require(set(releases) <= set(baseline["history"]) | {version}, "Unexpected release appeared")


def check_existing(app, manifest, notes, fetch):
    releases = history(app)
    version = manifest["version"]
    require(all(semver(v) <= semver(version) for v in releases), "A newer store version exists")
    release = releases.get(version)
    if release is None:
        return False
    require(release.get("is_published") is True, "Existing draft requires review")
    require(release.get("release_notes") == notes, "Existing release notes differ")
    data = fetch(release["pbw_url"])
    require(len(data) == manifest["pbw_bytes"] and digest(data) == manifest["pbw_sha256"],
            "Existing store version has different PBW bytes")
    return True


def public_pbw(url):
    import requests
    url = urljoin(API + "/", url)
    official = {
        "appstore-api.repebble.com", "assets.repebble.com"
    }
    # The official PBW endpoint redirects to a short-lived public R2 download.
    # This is separate from authenticated POSTs, which still reject redirects.
    storage = "pebble-appstore-backend.497e529f13ec4afbfce4dfe3cfd3634d.r2.cloudflarestorage.com"
    for hop in range(4):
        parsed = urlsplit(url)
        allowed = official if hop == 0 else official | {storage}
        require(parsed.scheme == "https" and parsed.netloc in allowed,
                "Unexpected PBW download host")
        # A fresh unauthenticated request per hop; no token or session cookies.
        with requests.get(url, timeout=30, allow_redirects=False, stream=True) as response:
            if response.status_code in (301, 302, 303, 307, 308):
                require(bool(response.headers.get("Location")), "PBW redirect missing destination")
                url = urljoin(url, response.headers["Location"])
                continue
            response_ok(response)
            chunks, size = [], 0
            for chunk in response.iter_content(65536):
                size += len(chunk)
                require(size <= 2_000_000, "PBW exceeds download budget")
                chunks.append(chunk)
            return b"".join(chunks)
    raise Stop("PBW download redirect limit exceeded")


def guarded_store_publisher(publisher, config, manifest, notes, post):
    expected = {"api_base", "app_id", "firebase_id_token", "pbw_path", "version",
                "release_notes", "is_published", "gif_paths", "screenshot_paths", "replace_screenshots"}
    require(set(inspect.signature(publisher._upload_release).parameters) == expected,
            "SDK uploader signature changed; inspect it before use")
    require(set(inspect.signature(publisher._post_with_wait_bar).parameters) ==
            {"url", "headers", "data", "files", "timeout", "label"},
            "SDK upload transport changed; inspect it before use")
    endpoint = API + "/api/dashboard/apps/" + config["store_app_id"] + "/releases"

    class Guarded(publisher):
        posted = False

        @classmethod
        def _post_with_wait_bar(cls, url, headers, data, files, timeout, label):
            require(not cls.posted and url == endpoint, "Duplicate or unexpected upload blocked")
            require(data == {"version": manifest["version"], "releaseNotes": notes,
                            "isPublished": "true", "replaceScreenshots": "false"},
                    "Unexpected upload fields")
            require(len(files) == 1 and files[0][0] == "pbwFile", "Only the frozen PBW may be uploaded")
            handle = files[0][1][1]
            require(digest(handle.read()) == manifest["pbw_sha256"], "Upload bytes differ from approval")
            handle.seek(0)
            cls.posted = True
            response = post(url, headers=headers, data=data, files=files,
                            timeout=60, allow_redirects=False)
            response_ok(response)
            return response
    return Guarded


def publish(folder, config, manifest, notes, session, token, publisher, post, fetch=public_pbw):
    before = dashboard_app(session, config)
    existing = check_existing(before, manifest, notes, fetch)
    baseline_path, attempt = folder / "store-baseline.json", folder / "store-attempt.json"
    identity = {"app_id": config["store_app_id"], "uuid": config["uuid"],
                "version": manifest["version"], "pbw_sha256": manifest["pbw_sha256"]}
    if not baseline_path.exists():
        require(not attempt.exists() and not existing, "Missing original baseline; reconcile previous attempt")
        with baseline_path.open("x") as handle:
            json.dump(dict(identity, metadata=preserved(before), history=history(before)), handle, indent=2)
    baseline = json.loads(baseline_path.read_text())
    require(all(baseline.get(k) == v for k, v in identity.items()), "Baseline belongs to another candidate")
    check_preserved(before, baseline, manifest["version"])
    if not existing:
        require(not attempt.exists(), "Previous upload is uncertain; read back before any retry, including browser fallback")
        guarded = guarded_store_publisher(publisher, config, manifest, notes, post)
        with attempt.open("x") as handle:
            json.dump(dict(identity, status="submitting"), handle, indent=2)
        guarded._upload_release(api_base=API, app_id=config["store_app_id"], firebase_id_token=token,
            pbw_path=str(folder / "popeye-gw.pbw"), version=manifest["version"], release_notes=notes,
            is_published=True, gif_paths=[], screenshot_paths=[], replace_screenshots=False)
    after = dashboard_app(session, config)
    check_preserved(after, baseline, manifest["version"])
    require(check_existing(after, manifest, notes, fetch), "Upload not visible yet; keep pending")
    attempt.write_text(json.dumps(dict(identity, status="dashboard-verified"), indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("candidate", type=Path, help="Frozen .release/X.Y.Z directory")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--inspect", action="store_true", help="Read authenticated listing without publishing")
    mode.add_argument("--publish", action="store_true", help="Publish using recorded exact-candidate approval")
    args = parser.parse_args()
    config = json.loads((ROOT / "docs/release-config.json").read_text())
    folder = args.candidate.resolve()
    require(folder.parent == ROOT / ".release", "Use a frozen .release/X.Y.Z candidate")
    manifest, notes = candidate(folder, config, args.publish)
    if not (args.inspect or args.publish):
        print("Local candidate checks passed; no network requests made.")
        return
    import requests
    from pebble_tool.commands.publish import PublishCommand
    session, token = dashboard_session()
    try:
        if args.inspect:
            app = dashboard_app(session, config)
            print(json.dumps({"id": app["id"], "uuid": app["app_uuid"],
                              "versions": list(history(app))}, indent=2))
        else:
            publish(folder, config, manifest, notes, session, token, PublishCommand, requests.post)
            print("Dashboard release/hash and listing preservation verified. Continue public catalog verification.")
    finally:
        session.close()


if __name__ == "__main__":
    try:
        main()
    except Stop as error:
        sys.exit(str(error))
    except Exception:
        # SDK/server exceptions may contain credentials or untrusted response text.
        sys.exit("Store helper failed. Check login, candidate files and remote state before retrying; no automatic resubmit.")
