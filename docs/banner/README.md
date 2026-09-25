# Banner

Bilder für die README und die Link-Vorschau im Stil der Firmware (Startbild, Neon Noir, Pixelschrift Spleen).

| Datei | Verwendung |
|---|---|
| `banner-dunkel.png` / `banner-hell.png` | Kopf der README – GitHub zeigt je nach Farbmodus des Lesers das passende (`<picture>`) |
| `vorschau.png` | Link-Vorschau: GitHub → *Settings* → *General* → *Social preview* → hochladen (1280 × 640) |

## Neu erzeugen

```bash
cd docs/banner
python3 make_banner.py        # braucht Pillow: pip install pillow
```

- `pixelbanner.py` – gemeinsame Bausteine: Farbwelten (`dunkel` = Neon Noir, `hell` = Neon Papier wie in den Oberflächen), Glitch-Schriftzug, Störstreifen, segmentierter Balken. Gezeichnet wird in grober Auflösung (320 × 80 bzw. 320 × 160) und danach 4-fach ohne Glättung vergrößert – so bleiben die Pixel scharf wie auf dem Display.
- `make_banner.py` – Aufbau der Bilder. Störstreifen mit festem Zufallswert: Das Ergebnis ist bei jedem Lauf gleich.
- Schrift nur in ihren Rastergrößen (Spleen 6×12 bei 12 px, 8×16 bei 16 px, 12×24 bei 24 px), sonst wird sie unscharf. Spleen 8×16 und 12×24 kennen kein „ ‚ – deshalb stehen im Banner keine deutschen Anführungszeichen.

Schrift: `fonts/` – Spleen 2.2.0, BSD-Lizenz (`fonts/LICENSE-spleen.txt`). Bilder unter `docs/` lösen keinen Firmware-Build aus (`paths-ignore` in `.github/workflows/firmware.yml`).
