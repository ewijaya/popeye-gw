---
name: popeye-gw-appstore
description: Prepares, registers, updates and verifies Popeye G&W's RePebble App Store listing (Games, Emery watchapp) through the approved release workflow, with durable App ID recording, lost-response recovery and preservation of existing metadata and artwork. Use when the user asks about the Popeye G&W store listing, App ID, Developer Dashboard, store description, icons, banner or screenshots, or verifying the public Emery catalog.
---

# Popeye G&W App Store

Read [release-config.json](../../../docs/release-config.json) and
[docs/releasing.md](../../../docs/releasing.md) sections 7–10 before deciding
whether a listing exists. `uuid` (`6b7c8c28-36f0-47ca-a073-3e8f33efcaf0`)
identifies the PBW. `store_app_id` is assigned by the store; while it is
`null`, the listing has not been recorded locally, which doesn't prove it
doesn't exist. Never invent an ID or reuse another project's listing or
artwork. Every store change runs inside the
[release workflow](../popeye-gw-release/SKILL.md) and its exact-PBW approval.

## Listing content

Prepared in M5 under `docs/releases/`, using the approved identity (PRD 9.4 and 14):

- `store-description.txt`: about 500 characters, describing the game with its
  approved Popeye, Olive Oyl and Brutus cast.
- `listing/`: a small and a large icon (144 × 144), a 720 × 320 Emery banner
  and up to five native 200 × 228 emulator screenshots of the PRD 9.4 scenes.
  PRD 9.4 lists a 48 × 48 small icon. The Dashboard that KasugaBus inspected on
  1 October 2026 recommended 80 × 80 and limited each file to 4.4 MB. Reinspect
  the current form and ask the owner about any mismatch; don't edit the PRD.
- Category Games, listed visibility, source and website set to the GitHub
  repository, no companion apps.

Check all listing text and art for the Popeye G&W name, the approved cast
and the nostalgic LCD presentation described in PRD section 14.
Freeze the description and each asset's SHA-256 with the candidate; never let
the store generate replacement icons.

## First registration

1. Discover first: check the public UUID catalog and the owner's logged-in
   Dashboard list. A match needs both UUID and source repository; a name is
   not enough. Adopt a match by recording its ID, then preserve it as an
   existing listing.
2. Before opening the form, write `.release/registration.json` with status
   `submitting`. A `submitting` or `uncertain` journal blocks another New.
3. Use Dashboard **New** (`/dashboard/submit`) in the owner's browser with the
   frozen PBW and listing. Check the extracted UUID, version, type and Emery
   hardware, and review the whole form before Submit. New publishes
   immediately, so submitting is the public release and needs the approval
   first.
4. Record the assigned ID the moment the response or App Information shows it,
   in the journal and in `store_app_id` / `store_listing_url`, before any other
   step.
5. If the response is lost, mark the journal `uncertain`, recover the ID by
   discovery, and never submit New again.

## Existing listing

Prefer the programmatic workflow in releasing.md section 9 using
`tools/upload_store.py`; routine release uploads need no browser. Use the
installed Pebble Tool's Python and existing `pebble login` credentials.
Browser fallback is for owner login, first registration, or an unsupported
operation after identifying the specific API/helper limitation. Reconcile an
uncertain upload before switching transports.

Read back the Dashboard API app; its ID and UUID must both match. Save the first
baseline (title, description, website, source, category, visibility,
companions, icons, banners, screenshots, every prior release) and keep it
across retries. A draft, a different PBW for the same version, or a newer
version stops the workflow for review. Upload only the frozen PBW and approved
notes through `tools/upload_store.py --publish` as detailed in releasing.md
section 9. It wraps the inspected SDK upload method without rebuilding or
replacing screenshots. Don't call the private uploader ad hoc. Confirm only the new release
changed. Never supply guessed empty defaults for fields you didn't read.

Change description or artwork only on an explicit request with the exact text
or files approved and hashed. The release helper deliberately doesn't edit
listing content. Prefer a separately inspected programmatic operation with
before/after read-backs; use Dashboard Edit Listing when that operation isn't
supported. Never reset the baseline to hide a difference.

## Login and credentials

The owner logs in to the Dashboard (and `pebble login` when the uploader
needs it). Never request passwords or tokens. Tokens stay in memory and go only
to `developer.repebble.com`, `appstore-api.repebble.com` and the official
sign-in provider. Refuse redirects when a token is attached.

## Verify

Check the general and `?hardware=emery` catalog at
`/api/v1/apps/uuid/{uuid}`. Each must have the matching ID and UUID, the latest
version, the frozen notes and an Emery platform (the platforms are objects).
The downloaded `pbw_file` must hash to the approved SHA-256, and the public
listing page must show the version and changelog. A successful upload is not
verification. Use two read-only attempts, then report pending surfaces. The
Emery catalog stands in for the phone's My Apps; claim phone freshness only
after the owner looks.
