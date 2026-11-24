# Weather icons

The 11 icons (clear day/night, partly cloudy day/night, cloudy, fog, drizzle, rain, snow,
thunder, unknown) at 24x24 and 48x48 px are **original artwork**, drawn programmatically by
`scripts/dev/make_icons.py` (circles, rounded clouds, capsule strokes and one polygon, on a
48-unit grid, rasterised with 4x4 supersampling and thresholded to 1 bit). No third-party
icon files, fonts or traces are used, so no external licence applies; the art is released
under the same licence as this repository.

Erik Flowers' Weather Icons (SIL OFL 1.1) were considered as a source but not used: traced
and thresholded to 24 px their thin strokes break up, and the generated set reads better on
the panel. Nothing from that project is included.

Regenerate with `python3 scripts/dev/make_icons.py` (standard library only; `--check`
verifies the committed `lib/epaper/src/IconsData.cpp` is current, `--preview sheet.png`
writes a contact sheet to look at). Output is deterministic.
