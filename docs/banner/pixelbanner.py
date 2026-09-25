"""Gemeinsame Bausteine fuer die README-Banner im Stil der Cardputer-Firmware.

Gezeichnet wird in grober Pixel-Aufloesung (wie auf dem 240x135-Display) mit der Pixelschrift
Spleen, danach wird das Bild ohne Glaettung vergroessert - so bleiben die Pixel scharf.
"""
import os
import random

from PIL import Image, ImageDraw, ImageFont

HIER = os.path.dirname(os.path.abspath(__file__))

# Farbwelten wie in den Oberflaechen: dunkel = Neon Noir, hell = Neon Papier
FARBEN = {
    'dunkel': dict(bg='#05070a', panel='#0b1016', dim='#1c2633', hint='#5a6b85', text='#9fb3c8',
                   bright='#e8f6ff', cyan='#05d9e8', magenta='#ff2a6d'),
    'hell': dict(bg='#eef2f5', panel='#ffffff', dim='#d3dbe3', hint='#6b7785', text='#2b3440',
                 bright='#05070a', cyan='#008c9c', magenta='#d4145a'),
}


def schrift(name, groesse):
    """Spleen in ihrer Rastergroesse (z.B. 12x24 bei 24 px) - nur so ist sie pixelgenau."""
    return ImageFont.truetype(os.path.join(HIER, 'fonts', name), groesse)


class Leinwand:
    def __init__(self, breite, hoehe, farben):
        self.f = farben
        self.img = Image.new('RGB', (breite, hoehe), farben['bg'])
        self.d = ImageDraw.Draw(self.img)
        self.d.fontmode = '1'   # keine Kantenglaettung: echte Pixel

    def text(self, x, y, s, font, farbe, faktor=1):
        """Text pixelgenau zeichnen, optional ganzzahlig vergroessert (grobe Pixel)."""
        l, t, r, b = font.getbbox(s)
        maske = Image.new('1', (r, b + 4), 0)
        md = ImageDraw.Draw(maske)
        md.fontmode = '1'
        md.text((0, 0), s, font=font, fill=1)
        if faktor != 1:
            maske = maske.resize((maske.width * faktor, maske.height * faktor), Image.NEAREST)
        self.img.paste(Image.new('RGB', maske.size, self.f[farbe]), (x, y), maske)
        return maske.width

    def glitch(self, x, y, s, font, faktor=1, versatz=1):
        """Schriftzug wie auf dem Startbild: Magenta links, Cyan rechts, hell darueber."""
        self.text(x - versatz, y, s, font, 'magenta', faktor)
        self.text(x + versatz, y, s, font, 'cyan', faktor)
        return self.text(x, y, s, font, 'bright', faktor)

    def stoerstreifen(self, anzahl, seed, bereich=None):
        """Duenne Stoerstreifen wie beim Einschalten (fester seed: Banner bleibt reproduzierbar)."""
        rnd = random.Random(seed)
        x0, y0, x1, y1 = bereich or (0, 0, self.img.width, self.img.height)
        for _ in range(anzahl):
            y = rnd.randrange(y0, y1)
            x = rnd.randrange(x0, x0 + (x1 - x0) // 2)
            laenge = rnd.randrange((x1 - x0) // 8, (x1 - x0) // 2)
            self.d.line([(x, y), (min(x1 - 1, x + laenge), y)], fill=self.f[rnd.choice(['cyan', 'magenta'])])

    def balken(self, x, y, breite, anteil, farbe='cyan'):
        """Segmentierter Fortschrittsbalken (3 px Segment, 1 px Luecke)."""
        self.d.rectangle([x, y, x + breite, y + 6], outline=self.f[farbe])
        voll = int((breite - 4) * anteil)
        for i in range(0, voll, 4):
            self.d.rectangle([x + 2 + i, y + 2, x + 2 + min(i + 2, voll - 1), y + 4], fill=self.f[farbe])

    def rahmen(self, x, y, b, h, farbe='dim', kante=None):
        self.d.rectangle([x, y, x + b - 1, y + h - 1], outline=self.f[farbe])
        if kante:
            self.d.rectangle([x, y, x + 1, y + h - 1], fill=self.f[kante])

    def speichern(self, pfad, faktor):
        self.img.resize((self.img.width * faktor, self.img.height * faktor), Image.NEAREST).save(pfad, optimize=True)
