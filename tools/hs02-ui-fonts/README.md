# HS-02 modern UI fonts

`gen_modern_font.py` bakes Inter (fonts/, SIL OFL 1.1, see OFL.txt) into
`source/Core/Threads/UI/drawing/color_160x80/Hs02ModernFont.hpp` for the colour
home / soldering / sleep screens (`draw_home_gauge.cpp`).

Re-run it after changing any text in `Hs02UiStrings.hpp` (only the glyphs used there
are embedded):

```sh
pip install pillow
python3 tools/hs02-ui-fonts/gen_modern_font.py
```

Language: Russian by default; define `HS02_UI_LANG_EN` (in `Hs02UiStrings.hpp` or via
CFLAGS) for English, then re-run the generator.
