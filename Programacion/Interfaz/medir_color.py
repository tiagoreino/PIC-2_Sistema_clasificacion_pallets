"""Herramienta para calibrar colores.

Uso:  python medir_color.py [foto]      (por defecto foto_capturada.jpg)

Muestra el RGB promedio del centro de la foto (donde deberia estar el pallet),
cuantos pixeles toma cada color con las reglas de identificacion.py, y guarda
mascara_colores.png con los pixeles detectados pintados (amarillo = ROJO, cian = AZUL).
"""
import os
import sys

from PIL import Image

import identificacion as idf

ruta = sys.argv[1] if len(sys.argv) > 1 else "foto_capturada.jpg"
img = Image.open(ruta).convert("RGB")
ancho, alto = img.size
pix = img.load()

# RGB promedio del 20 % central
x0, x1 = int(ancho * 0.4), int(ancho * 0.6)
y0, y1 = int(alto * 0.4), int(alto * 0.6)
suma = [0, 0, 0]
n = 0
for x in range(x0, x1):
    for y in range(y0, y1):
        for c in range(3):
            suma[c] += pix[x, y][c]
        n += 1
r, g, b = (s // n for s in suma)
print(f"Foto: {ruta} ({ancho}x{alto})")
print(f"RGB promedio del centro: R={r} G={g} B={b}")
print(f"  ROJO: G/R={g / max(r, 1):.2f} (max {idf.ROJO_MAX_G})  B/R={b / max(r, 1):.2f} (max {idf.ROJO_MAX_B})"
      f"  R-max(G,B)={r - max(g, b)} (min {idf.DIF_MIN})")
print(f"  AZUL: R/B={r / max(b, 1):.2f} (max {idf.AZUL_MAX_R})  G/B={g / max(b, 1):.2f} (max {idf.AZUL_MAX_G})"
      f"  B-R={b - r} (min {idf.DIF_MIN})")
print(f"  Brillo minimo pedido: {idf.BRILLO_MIN}")

# Mascara de toda la foto
mascara = img.copy()
pm = mascara.load()
cuenta = {"ROJO": 0, "AZUL": 0}
for x in range(ancho):
    for y in range(alto):
        if idf._cumple_color(*pix[x, y], "ROJO"):
            pm[x, y] = (255, 255, 0)
            cuenta["ROJO"] += 1
        elif idf._cumple_color(*pix[x, y], "AZUL"):
            pm[x, y] = (0, 255, 255)
            cuenta["AZUL"] += 1

print(f"Pixeles que pasan: ROJO={cuenta['ROJO']}  AZUL={cuenta['AZUL']}")
salida = os.path.join(os.path.dirname(os.path.abspath(ruta)), "mascara_colores.png")
mascara.save(salida)
print(f"Mascara guardada en {salida}")
