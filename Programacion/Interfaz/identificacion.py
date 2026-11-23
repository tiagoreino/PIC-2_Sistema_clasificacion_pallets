import os
from PIL import Image, ImageEnhance, ImageFilter, ImageDraw, ImageFont

UMBRAL_PIXELES = 10     # minimo de pixeles para considerar que hay objeto (ignora ruido)
MARGEN_FORMA = 15       # diferencia ancho/alto para distinguir cuadrado/circulo de rectangulo

# ASUMIDO: cada tipo de pallet corresponde a un color
PALLET_A_COLOR = {
    "Rojo": "ROJO",
    "Azul": "AZUL",
}

# Calibracion px -> mm (regla de 3): un objeto de REF_MM mide REF_PX en la foto
# Calibracion px -> mm (regla de 3) separada para cada eje
REF_MM_X = 50.0
REF_PX_X = 104.0
REF_MM_Y = 40.0
REF_PX_Y = 83.0   # alto en la foto (px)

# Dimensiones esperadas (ancho, alto) en mm de cada tipo de pallet y tolerancia
PALLET_A_DIMENSIONES = {
    "Rojo": (50.0, 40.0),
    "Azul": (50.0, 40.0),
}
TOLERANCIA_MM = 5.0


def _px_a_mm(px, eje="x"):
    if eje == "x":
        return px * REF_MM_X / REF_PX_X
    return px * REF_MM_Y / REF_PX_Y 


def _cumple_dimensiones(ancho_mm, alto_mm, dimensiones):
    if dimensiones is None:
        return True
    esp_ancho, esp_alto = dimensiones
    return (abs(ancho_mm - esp_ancho) <= TOLERANCIA_MM and
            abs(alto_mm - esp_alto) <= TOLERANCIA_MM)


# Color por proporcion entre canales (no por valor absoluto): aguanta cambios de luz.
# Ej: rojo = el canal R manda y G y B son menos de la mitad de R.
BRILLO_MIN = 110         # canal principal minimo (descarta zonas oscuras o sombras)
DIF_MIN = 60             # el canal principal le saca al menos esto al segundo (descarta grises)
ROJO_MAX_G = 0.62        # G < 0.62 * R  (deja afuera el naranja)
ROJO_MAX_B = 0.55        # B < 0.55 * R
AZUL_MAX_R = 0.60        # R < 0.60 * B
AZUL_MAX_G = 0.85        # G < 0.85 * B
ROJO_BRILLO_MIN = 150    # el rojo pide mas brillo: el reflejo del laser en el fondo es rojo oscuro
FRACCION_PERFIL = 0.5    # columna/fila cuenta como pallet si tiene al menos la mitad de la mas llena


def _cumple_color(r, g, b, color):
    if color == "ROJO":
        return (r >= max(BRILLO_MIN, ROJO_BRILLO_MIN) and r - max(g, b) >= DIF_MIN and
                g < r * ROJO_MAX_G and b < r * ROJO_MAX_B)
    if color == "AZUL":
        return (b >= BRILLO_MIN and b - r >= DIF_MIN and
                r < b * AZUL_MAX_R and g < b * AZUL_MAX_G)
    return False


def _mancha_mas_grande(mascara, ancho, alto):
    mejor = (0, 0, 0, 0, 0)
    mejor_pixeles = []
    visto = bytearray(ancho * alto)

    for inicio in range(ancho * alto):
        if not mascara[inicio] or visto[inicio]:
            continue

        pila = [inicio]
        visto[inicio] = 1
        cant = 0
        x0, y0, x1, y1 = ancho, alto, 0, 0
        pixeles = []

        while pila:
            i = pila.pop()
            y, x = divmod(i, ancho)
            cant += 1
            pixeles.append(i)
            x0, x1 = min(x0, x), max(x1, x)
            y0, y1 = min(y0, y), max(y1, y)

            for j in (i - 1 if x > 0 else -1, i + 1 if x < ancho - 1 else -1,
                      i - ancho, i + ancho):
                if 0 <= j < ancho * alto and mascara[j] and not visto[j]:
                    visto[j] = 1
                    pila.append(j)

        if cant > mejor[0]:
            mejor = (cant, x0, y0, x1, y1)
            mejor_pixeles = pixeles

    if not mejor_pixeles:
        return mejor

    # Recorte por perfil: se descartan columnas y filas con pocos pixeles
    # (brillo del laser, reflejos o bordes que se pegan al pallet)
    cols = [0] * ancho
    filas = [0] * alto
    for i in mejor_pixeles:
        y, x = divmod(i, ancho)
        cols[x] += 1
        filas[y] += 1

    lim_c = max(cols) * FRACCION_PERFIL
    lim_f = max(filas) * FRACCION_PERFIL
    xs = [x for x in range(ancho) if cols[x] >= lim_c]
    ys = [y for y in range(alto) if filas[y] >= lim_f]

    return (mejor[0], xs[0], ys[0], xs[-1], ys[-1])


def procesar_y_detectar(ruta_imagen, ruta_salida, color_buscado="NEGRO", dimensiones=None):
    """
    Filtra la imagen, busca el color pedido y guarda en ruta_salida la imagen
    filtrada (con recuadro rojo si encontro el objeto).
    Devuelve un diccionario con el resultado.
    """
    if not os.path.exists(ruta_imagen):
        raise FileNotFoundError(f"No existe el archivo {ruta_imagen}")

    with Image.open(ruta_imagen) as original:
        img = original.convert("RGB")
    ancho, alto = img.size

    # Nitidez y contraste para mejorar el analisis
    filtrada = img.filter(ImageFilter.SHARPEN)
    img_filtros = ImageEnhance.Contrast(filtrada).enhance(1.2)

    ruta_filtrada = os.path.splitext(ruta_salida)[0] + "_filtros.png"
    img_filtros.save(ruta_filtrada)

    # Segmentacion por color
    pix = img_filtros.load()
    mascara = bytearray(ancho * alto)

    for x in range(ancho):
        for y in range(alto):
            r, g, b = pix[x, y]
            if _cumple_color(r, g, b, color_buscado):
                mascara[y * ancho + x] = 1

    # Se queda con la mancha mas grande del color (el pallet); lo demas es ruido o fondo
    pixeles, x_min, y_min, x_max, y_max = _mancha_mas_grande(mascara, ancho, alto)

    resultado = {
        "detectado": pixeles > UMBRAL_PIXELES,
        "color": color_buscado,
        "pixeles": pixeles,
        "ancho_objeto": 0,
        "alto_objeto": 0,
        "forma": None,
        "ancho_mm": 0.0,
        "alto_mm": 0.0,
        "cumple_dimensiones": False,
        "ruta_imagen": ruta_salida,
    }

    img_salida = img_filtros.copy()

    if resultado["detectado"]:
        ancho_obj = x_max - x_min
        alto_obj = y_max - y_min
        resultado["ancho_objeto"] = ancho_obj
        resultado["alto_objeto"] = alto_obj
        resultado["forma"] = (
            "Cuadrado o Círculo"
            if abs(ancho_obj - alto_obj) < MARGEN_FORMA
            else "Rectángulo"
        )

        ancho_mm = _px_a_mm(ancho_obj + 1, "x")
        alto_mm = _px_a_mm(alto_obj + 1, "y")
        cumple = _cumple_dimensiones(ancho_mm, alto_mm, dimensiones)
        resultado["ancho_mm"] = round(ancho_mm, 1)
        resultado["alto_mm"] = round(alto_mm, 1)
        resultado["cumple_dimensiones"] = cumple

        # Verde si cumple las dimensiones, rojo si no
        color_recuadro = (0, 255, 0) if cumple else (255, 0, 0)

        # Grosor y letra proporcionales a la foto (QVGA de la camara o fotos grandes)
        grosor = max(3, min(ancho, alto) // 100)
        tam_letra = max(14, min(ancho, alto) // 15)

        dibujo = ImageDraw.Draw(img_salida)
        dibujo.rectangle([(x_min, y_min), (x_max, y_max)],
                         outline=color_recuadro, width=grosor)

        # Medidas arriba del recuadro (o adentro si no entra arriba)
        texto = f"{ancho_mm:.0f} x {alto_mm:.0f} mm"
        try:
            fuente = ImageFont.load_default(size=tam_letra)
        except TypeError:
            fuente = ImageFont.load_default()
        caja = dibujo.textbbox((0, 0), texto, font=fuente)
        alto_texto = caja[3] - caja[1] + 4
        y_texto = y_min - alto_texto - 2 if y_min - alto_texto - 2 >= 0 else y_min + 4
        dibujo.rectangle([(x_min, y_texto), (x_min + caja[2] - caja[0] + 6, y_texto + alto_texto)],
                         fill=color_recuadro)
        dibujo.text((x_min + 3, y_texto + 1), texto, fill=(0, 0, 0), font=fuente)

    # Escritura atomica, porque la interfaz lee este archivo cada 300 ms
    ruta_tmp = ruta_salida + ".tmp"
    img_salida.save(ruta_tmp, format="JPEG")
    os.replace(ruta_tmp, ruta_salida)

    return resultado


def criterio_aceptacion(resultado):
    # El objeto es un pallet valido si tiene un color conocido y las medidas de ese color
    return resultado["detectado"] and resultado["cumple_dimensiones"]


def evaluar_pallet(ruta_foto, ruta_salida, tipo_pallet):
    # Devuelve (aceptado, resultado). Lo llama la interfaz.
    # aceptado: True = es el tipo elegido, False = es otro tipo de pallet valido,
    # None = objeto extrano (ningun color conocido o no cumple las medidas de su color)
    mejor = None
    rutas = []

    # Se prueban todos los colores: el que tiene mas pixeles dice que objeto es
    for tipo, color in PALLET_A_COLOR.items():
        ruta_color = os.path.splitext(ruta_salida)[0] + f"_{color}.jpg"
        r = procesar_y_detectar(ruta_foto, ruta_color, color, PALLET_A_DIMENSIONES.get(tipo))
        r["tipo"] = tipo
        rutas.append(ruta_color)
        if mejor is None or r["pixeles"] > mejor["pixeles"]:
            mejor = r

    os.replace(mejor["ruta_imagen"], ruta_salida)
    for ruta in rutas:
        if os.path.exists(ruta):
            os.remove(ruta)
    mejor["ruta_imagen"] = ruta_salida

    if not criterio_aceptacion(mejor):
        return None, mejor
    return mejor["tipo"] == tipo_pallet, mejor


if __name__ == "__main__":
    base = (r"c:/Users/tiago/OneDrive/Documentos/UTEC/PIC II/"
            r"PIC-2_Sistema_clasificacion_pallets/Programacion/Procesamiento de Imagen")
    salida = base + "/Prueba de identificacion"

    for nombre, color in (("pallet_rojo", "ROJO"),
                          ("pallet_azul", "AZUL")):
        r = procesar_y_detectar(f"{base}/{nombre}.jpg",
                                f"{salida}/imagen_detectada_{nombre}.jpg", color)
        print(nombre, r)