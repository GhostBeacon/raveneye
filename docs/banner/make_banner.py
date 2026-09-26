"""README-Banner und Vorschaubild fuer Krähenauge (siehe README.md in diesem Ordner).

    python3 make_banner.py        -> banner-dunkel.png, banner-hell.png, vorschau.png
"""
import os

from pixelbanner import FARBEN, HIER, Leinwand, schrift

S6, S8, S12 = schrift('spleen-6x12.otf', 12), schrift('spleen-8x16.otf', 16), schrift('spleen-12x24.otf', 24)
FAKTOR = 4   # Pixel-Leinwand wird 4-fach vergroessert


def banner(modus):
    """1280 x 320: Schriftzug links, Statuszeilen rechts - wie das Startbild der Firmware."""
    c = Leinwand(320, 80, FARBEN[modus])
    c.stoerstreifen(5, seed=7, bereich=(0, 2, 320, 10))
    c.glitch(10, 10, 'KRÄHENAUGE', S12, faktor=2)   # 240 px breit
    c.text(16, 62, '// Monitoring für den Cardputer ADV', S6, 'hint')
    x = 264   # Statusbox rechts (schmal, der Schriftzug braucht 240 px): Zeilen wie beim Start, darunter der Balken
    c.rahmen(x - 6, 10, 60, 62, 'dim', kante='magenta')
    for i, zeile in enumerate(['> wlan', '> ntfy', '> status', '> online']):
        c.text(x, 13 + i * 11, zeile, S6, 'magenta' if i == 3 else 'text')
    c.balken(x - 2, 60, 52, 1.0)
    return c


def vorschau():
    """1280 x 640 fuer die Link-Vorschau (GitHub: Settings -> Social preview), immer dunkel."""
    c = Leinwand(320, 160, FARBEN['dunkel'])
    c.stoerstreifen(9, seed=3, bereich=(0, 4, 320, 30))
    breite = S12.getbbox('KRÄHENAUGE')[2] * 2
    c.glitch((320 - breite) // 2, 34, 'KRÄHENAUGE', S12, faktor=2)
    sub = '// watching the net'
    c.text((320 - S8.getbbox(sub)[2]) // 2, 90, sub, S8, 'hint')
    c.balken(80, 116, 160, 0.72)
    c.text(80, 128, '> ntfy · krähenwacht · kolonie', S6, 'magenta')
    fuss = 'Teil von CORVUS'
    c.text(320 - 8 - S6.getbbox(fuss)[2], 146, fuss, S6, 'hint')
    return c


if __name__ == '__main__':
    for m in FARBEN:
        banner(m).speichern(os.path.join(HIER, f'banner-{m}.png'), FAKTOR)
    vorschau().speichern(os.path.join(HIER, 'vorschau.png'), FAKTOR)
    print('banner-dunkel.png, banner-hell.png, vorschau.png geschrieben')
