# HS-02 ready-screen pictures

`presets/preset1.png` … `preset3.png` are shown on the Ready screen while the target equals
*Settings → Tip → Preset 1 … 3*; `presets/standby.png` on the standby screen after power-on.
Replace them with any pictures (PNG, JPG, …) and run:

```sh
pip install pillow numpy
python3 tools/hs02-ui-images/gen_preset_images.py
```

The script scales each picture to cover 160×64 (centre crop) and writes
`source/Core/Threads/UI/drawing/color_160x80/Hs02PresetImages.hpp`. The LCD frame has only
4 colours per row band, so every 4-row band of a picture gets its own 4 colours; the result is
saved next to the source as `presets/preview_*.png` to check before flashing.

Pictures with few colours, clear shapes and a dark background look best (pixel art is ideal).
The picture covers the top 64 rows; the bottom 16 rows are a black strip with the temperature,
the preset number and the heater power (standby: the hint "OK — нагрев").
