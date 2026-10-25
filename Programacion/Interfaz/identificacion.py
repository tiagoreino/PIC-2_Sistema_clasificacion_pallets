import os
from PIL import Image, ImageEnhance, ImageFilter, ImageDraw

UMBRAL_PIXELES = 10     # minimo de pixeles para considerar que hay objeto (ignora ruido)
MARGEN_FORMA = 15       # diferencia ancho/alto para distinguir cuadrado/circulo de rectangulo

# ASUMIDO: cada tipo de pallet corresponde a un color
PALLET_A_COLOR = {
    "Rojo": "ROJO",
    "Azul": "AZUL",
}


def _cumple_color(r, g, b, color):
    if color == "ROJO":
        return r > 230 and g < 180 and b < 200
    if color == "AZUL":
        return b > 230 and g < 180 and r < 200
    return False


def procesar_y_detectar(ruta_imagen, ruta_salida, color_buscado="NEGRO"):
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
    x_min, y_min = ancho, alto
    x_max, y_max = 0, 0
    pixeles = 0

    for x in range(ancho):
        for y in range(alto):
            r, g, b = pix[x, y]
            if _cumple_color(r, g, b, color_buscado):
                pixeles += 1
                x_min = min(x_min, x)
                x_max = max(x_max, x)
                y_min = min(y_min, y)
                y_max = max(y_max, y)

    resultado = {
        "detectado": pixeles > UMBRAL_PIXELES,
        "color": color_buscado,
        "pixeles": pixeles,
        "ancho_objeto": 0,
        "alto_objeto": 0,
        "forma": None,
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

        dibujo = ImageDraw.Draw(img_salida)
        dibujo.rectangle([(x_min, y_min), (x_max, y_max)],
                         outline=(0, 255, 0), width=3)

    # Escritura atomica, porque la interfaz lee este archivo cada 300 ms
    ruta_tmp = ruta_salida + ".tmp"
    img_salida.save(ruta_tmp, format="JPEG")
    os.replace(ruta_tmp, ruta_salida)

    return resultado


def criterio_aceptacion(resultado):
    # ASUMIDO: el pallet es conforme si se detecto el color que corresponde a su tipo.
    return resultado["detectado"]


def evaluar_pallet(ruta_foto, ruta_salida, tipo_pallet):
    # Devuelve (aceptado: bool, resultado: dict). Lo llama la interfaz.
    color = PALLET_A_COLOR[tipo_pallet]
    resultado = procesar_y_detectar(ruta_foto, ruta_salida, color)
    return criterio_aceptacion(resultado), resultado


if __name__ == "__main__":
    base = (r"c:/Users/tiago/OneDrive/Documentos/UTEC/PIC II/"
            r"PIC-2_Sistema_clasificacion_pallets/Programacion/Procesamiento de Imagen")
    salida = base + "/Prueba de identificacion"

    for nombre, color in (("pallet_rojo", "ROJO"),
                          ("pallet_azul", "AZUL")):
        r = procesar_y_detectar(f"{base}/{nombre}.jpg",
                                f"{salida}/imagen_detectada_{nombre}.jpg", color)
        print(nombre, r)