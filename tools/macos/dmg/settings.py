# dmgbuild settings for the Street Rod Enhanced disk image (tools/macos/make-dmg.sh runs it).
#   dmgbuild -s tools/macos/dmg/settings.py -D app="dist/Street Rod Enhanced.app" -D license=LICENSE \
#     "Street Rod Enhanced <version>" dist/Street-Rod-Enhanced.dmg
import os
import subprocess
import tempfile

# dmgbuild does not define __file__; run from the repo root or pass -D assets=DIR.
here = os.path.abspath(defines.get("assets", "tools/macos/dmg"))  # noqa: F821
app = defines["app"]  # noqa: F821 - provided by dmgbuild
license_file = defines.get("license", "LICENSE")  # noqa: F821
app_name = os.path.basename(app.rstrip("/"))

# HiDPI background: 1x and 2x PNGs combined into one multi-representation TIFF.
_tiff = os.path.join(tempfile.mkdtemp(prefix="sre-dmg-"), "background.tiff")
subprocess.check_call([
    "tiffutil", "-cathidpicheck",
    os.path.join(here, "background.png"),
    os.path.join(here, "background@2x.png"),
    "-out", _tiff,
])

format = "UDZO"
files = [app, license_file]
symlinks = {"Applications": "/Applications"}
background = _tiff
show_status_bar = False
show_tab_view = False
show_toolbar = False
show_pathbar = False
show_sidebar = False
window_rect = ((200, 120), (760, 628))
default_view = "icon-view"
icon_size = 86
text_size = 12
# Matching the boxes and the arrow drawn in background.html; LICENSE in the free corner on the right.
icon_locations = {app_name: (119, 95), "Applications": (339, 95), os.path.basename(license_file): (680, 455)}
