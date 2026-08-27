#!/usr/bin/env python3
"""Заливка AAB в Google Play (production, staged). Использование:
   play_publish.py status
   play_publish.py upload <path.aab> --fraction 0.2 [--notes "текст"]
   play_publish.py rollout <versionCode> --fraction 0.5|1.0   (1.0 = completed)
Ключ: /Users/vh/.nvovpn_keys/play-service-account.json, пакет com.nvovpn.app."""
import sys, argparse
from google.oauth2 import service_account
from googleapiclient.discovery import build
from googleapiclient.http import MediaFileUpload
PKG = "com.nvovpn.app"; KEY = "/Users/vh/.nvovpn_keys/play-service-account.json"
def svc():
    creds = service_account.Credentials.from_service_account_file(KEY, scopes=["https://www.googleapis.com/auth/androidpublisher"])
    return build("androidpublisher", "v3", credentials=creds, cache_discovery=False)
def status(s):
    e = s.edits().insert(packageName=PKG, body={}).execute()["id"]
    for track in ("production", "beta", "internal"):
        t = s.edits().tracks().get(packageName=PKG, editId=e, track=track).execute()
        for r in t.get("releases", []):
            print(track, "|", r.get("status"), "| versionCodes", r.get("versionCodes"), "| name", r.get("name"), "| userFraction", r.get("userFraction"))
    s.edits().delete(packageName=PKG, editId=e).execute()
def upload(s, path, fraction, notes):
    e = s.edits().insert(packageName=PKG, body={}).execute()["id"]
    b = s.edits().bundles().upload(packageName=PKG, editId=e, media_body=MediaFileUpload(path, mimetype="application/octet-stream", resumable=True)).execute()
    vc = b["versionCode"]; print("uploaded versionCode", vc)
    release = {"versionCodes": [str(vc)], "status": "inProgress" if fraction < 1.0 else "completed"}
    if fraction < 1.0: release["userFraction"] = fraction
    if notes: release["releaseNotes"] = [{"language": "ru-RU", "text": notes}]
    s.edits().tracks().update(packageName=PKG, editId=e, track="production", body={"track": "production", "releases": [release]}).execute()
    s.edits().commit(packageName=PKG, editId=e).execute(); print("committed: production", release["status"], "fraction", fraction)
def rollout(s, vc, fraction):
    e = s.edits().insert(packageName=PKG, body={}).execute()["id"]
    release = {"versionCodes": [str(vc)], "status": "inProgress" if fraction < 1.0 else "completed"}
    if fraction < 1.0: release["userFraction"] = fraction
    s.edits().tracks().update(packageName=PKG, editId=e, track="production", body={"track": "production", "releases": [release]}).execute()
    s.edits().commit(packageName=PKG, editId=e).execute(); print("committed:", vc, release["status"], "fraction", fraction)
ap = argparse.ArgumentParser(); sub = ap.add_subparsers(dest="cmd", required=True)
sub.add_parser("status")
u = sub.add_parser("upload"); u.add_argument("aab"); u.add_argument("--fraction", type=float, default=0.2); u.add_argument("--notes", default="")
r = sub.add_parser("rollout"); r.add_argument("versionCode", type=int); r.add_argument("--fraction", type=float, required=True)
a = ap.parse_args(); s = svc()
{"status": lambda: status(s), "upload": lambda: upload(s, a.aab, a.fraction, a.notes), "rollout": lambda: rollout(s, a.versionCode, a.fraction)}[a.cmd]()
