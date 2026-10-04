from PIL import Image
from PIL import ImageEnhance
from PIL import ImageFilter
from PIL import ImageDraw
import os

# Función para procesar, filtrar y detectar objetos en CUALQUIER imagen
def procesar_y_detectar(ruta_imagen, nombre_salida, color_buscado="NEGRO"):

    # Cargar imagen de forma segura
    if not os.path.exists(ruta_imagen):
        print(f"Error: No existe el archivo {ruta_imagen}")
        return

    img = Image.open(ruta_imagen).convert("RGB")
    ancho, alto = img.size
    print(f"\nProcesando [{nombre_salida}] -> Tamaño: {ancho}x{alto}")

    # Aplicar filtros para mejor analisis (Nitidez y Contraste)
    filtrada = img.filter(ImageFilter.SHARPEN)
    enhancer = ImageEnhance.Contrast(filtrada)
    img_filtros = enhancer.enhance(1.2)
    
    # Guardar imagen filtrada
    ruta_base_salida = r"c:/Users/tiago/OneDrive/Documentos/UTEC/PIC II/PIC-2_Sistema_clasificacion_pallets/Programacion/Procesamiento de Imagen/Prueba de identificacion"
    img_filtros.save(os.path.join(ruta_base_salida, f"imagen_filtros_{nombre_salida}.jpg"))

    # Analizar pixeles (Segmentación por color)
    x_min, y_min = ancho, alto
    x_max, y_max = 0, 0
    pixeles_detectados = 0

    for x in range(ancho):
        for y in range(alto):
            r, g, b = img_filtros.getpixel((x, y))
            
            cumple_filtro = False
            
            
            if color_buscado == "NEGRO":
                # Píxeles oscuros (menores a 50)
                if r < 50 and g < 50 and b < 50:
                    cumple_filtro = True
                    
            elif color_buscado == "VERDE":
                # Mucho verde, poco rojo y azul
                if g > 250 and r < 220 and b > 240:
                    cumple_filtro = True
                    
            elif color_buscado == "AMARILLO":
                # Mucho rojo y verde, poco azul
                if r > 210 and g > 250 and b < 195:
                    cumple_filtro = True

            # Si el pixel coincide, expandimos la caja de detección
            if cumple_filtro:
                pixeles_detectados += 1
                if x < x_min: x_min = x
                if x > x_max: x_max = x
                if y < y_min: y_min = y
                if y > y_max: y_max = y

    # Dibujar recuadro si se encontro el objeto
    if pixeles_detectados > 10:  # Minimo 10 píxeles para ignorar ruido falso
        ancho_objeto = x_max - x_min
        alto_objeto = y_max - y_min

        # Clonamos la filtrada para dibujar encima el reborde
        img_detectada = img_filtros.copy()
        dibujo = ImageDraw.Draw(img_detectada)
        
        # Rectángulo ROJO de grosor 10
        dibujo.rectangle([(x_min, y_min), (x_max, y_max)], outline=(255, 0, 0), width=10)
        img_detectada.save(os.path.join(ruta_base_salida, f"imagen_detectada_{nombre_salida}.jpg"))
        
        print(f"-> {color_buscado} detectado ({pixeles_detectados} píxeles). Objeto: {ancho_objeto}x{alto_objeto} px")
        
        # Geometria
        diferencia = abs(ancho_objeto - alto_objeto)
        if diferencia < 15: # Subi el margen de error a 15 por si la foto esta de perspectiva
            print("-> Forma estimada: Cuadrado o Círculo")
        else:
            print("-> Forma estimada: Rectángulo")
    else:
        print(f"-> No se detectó suficiente color {color_buscado} en esta imagen.")



# Definimos las rutas 
ruta_cargador = r"c:/Users/tiago/OneDrive/Documentos/UTEC/PIC II/PIC-2_Sistema_clasificacion_pallets/Programacion/Procesamiento de Imagen/Cargador.jpg"
ruta_verde = r"c:/Users/tiago/OneDrive/Documentos/UTEC/PIC II/PIC-2_Sistema_clasificacion_pallets/Programacion/Procesamiento de Imagen/PostIt_Verde.jpg"
ruta_amarillo = r"c:/Users/tiago/OneDrive/Documentos/UTEC/PIC II/PIC-2_Sistema_clasificacion_pallets/Programacion/Procesamiento de Imagen/PostIt_Amarillo.jpg"

# Ejecutamos el analisis individual para cada una especificando que buscar
procesar_y_detectar(ruta_cargador, "cargador", color_buscado="NEGRO")
procesar_y_detectar(ruta_verde, "verde", color_buscado="VERDE")
procesar_y_detectar(ruta_amarillo, "amarillo", color_buscado="AMARILLO")
