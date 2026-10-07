# Phone configuration and data

Connection alerts and scene weather effects are retired. Their wire IDs remain
reserved, but old saved values are sanitized to false and neither control
appears in the form.

The Clock uses pinned `@rebble/clay` 1.1.0. Its settings page is generated into
an offline data URL; no configuration server is needed on a real phone. The
emulator keeps Clay's standard proxy/return flow. `app.js` handles saves itself
so that sanitized, versioned preferences enter one bounded AppMessage queue.
Phone-only location and IANA zone strings never enter settings messages. Even a
complete settings snapshot fits below 600 bytes; C opens a 1024-byte inbox.
Saves compare against acknowledged values so a failed earlier save is retried
with later edits. Delivery retries back off five times, and the next phone-ready
event resends the complete saved preferences without reapplying a preset.

Each preset resets presentation controls to defaults and preserves weather
consent, location, units, refresh period, step goal, personal labels, event date,
quiet-hour/power limits. The form updates before saving:

| Preset | Rows | Additional presentation |
| --- | --- | --- |
| Classic | Hidden / hidden | Ivory LCD |
| Everyday | Date / battery | Defaults, Ivory LCD |
| Traveller | World / date | Saved world zone |
| Large | Date / battery | Larger digits, high contrast |

Animation activity, speed, pauses, character selection, reduced motion are independent and survive preset changes. Lively therefore
works with Classic. Removed Active (wire value 3) migrates to Custom while
retaining its saved layout/motion. Japanese is date-language value 4; the watch
renders seven compact kanji glyphs without adding a system-font dependency.

Changing a presentation control marks the preset Custom. Opening the page never
reapplies a saved preset. The Apply button can reapply a selected preset.

Weather is disabled by default, including with Traveller. Enabling it allows
HTTPS requests to Open-Meteo's free, noncommercial forecast endpoint. Automatic
mode uses the phone's coarse location; manual mode uses Open-Meteo's GeoNames
city/postal-code search. The Weather section includes attribution and explains
the location disclosure. There is no API key. Current temperature, today's
high/low, WMO weather code and the model's current timestamp are validated and
cached. Temperatures stay in Celsius in phone storage and are converted into
the selected unit when sent, with an explicit WeatherUnit. Failures preserve
the previous successful timestamp and mark the data stale. Location changes
cancel/ignore older requests. Attempts are at least one minute apart, successful
updates obey the selected 15–180 minute period, and errors retry after five
minutes. GPS and HTTP calls have explicit timeouts.

World time uses `moment` 2.30.1 and `moment-timezone` 0.6.5 rather than `Intl`.
`timezone-data.json` contains 29 curated IANA 2026e zones, bounded to 2020–2040.
Offsets refresh on phone ready, settings save, watch request, daily, and one
second after the next DST transition. Outside the bundled range the timestamp
becomes zero, so the watch displays unavailable instead of guessing an offset.
Future timezone law changes require updating the dependency and regenerating
the data using `tests/generate_js_timezones.js`.

`clay-compat.js` adapts only the known `let value`, `let integerValue` and
`let currentValue` declarations shipped in Clay 1.1.0's embedded val/radiogroup
manipulators. They become function-scoped `var` declarations for old WebViews.
`config-custom.js` also supplies the missing integer-helper methods before the
page builds. The installed package is untouched. Both the assembled HTML and
the SDK webpack phone bundle were checked with the installed SDK's ES5 parser;
the complete bundle was also exercised with `Intl` unavailable.

The Waf bundle hook keeps `build/pebble-js-app.js.map` as a local debug artifact
and excludes it from the PBW. SDK 4.33.1 normally packages every merged JS output;
the source map is optional at runtime and its removal keeps the PBW below the
repository's unchanged 2,000,000-byte package budget.

Run all phone tests without network/GPS/browser access from `watchface/`:

```sh
/Users/e_wijaya_ap/.nvm/versions/node/v20.19.5/bin/node tests/test_pkjs.js
```

That existing Node executable was checked and reports v20.19.5. Dependency
installation is local and reproducible with `npm ci --ignore-scripts` and the
checked-in lockfile. Tests use the actual compiled Clay package in an isolated
VM, mock page events, time, AppMessage acknowledgements, GPS and HTTP. They
cover settings round trips/corruption, preset behavior, retry exhaustion,
weather opt-in/races/cache/units/timeouts and DST transitions. The parent task
owns full SDK builds and emulator/device verification.

Primary references:

- [RePebble app configuration guide](https://developer.repebble.com/guides/user-interfaces/app-configuration/)
- [Clay source](https://github.com/pebble-dev/clay)
- [Open-Meteo forecast API](https://open-meteo.com/en/docs)
- [Open-Meteo geocoding API](https://open-meteo.com/en/docs/geocoding-api)
- [Moment Timezone documentation](https://momentjs.com/timezone/docs/)
