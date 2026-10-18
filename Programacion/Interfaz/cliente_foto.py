import os
import socket
import struct
 
PORT = 5000
TIMEOUT = 30.0
TAMANO_MAXIMO = 2_000_000   # bytes; mas que esto se considera un encabezado corrupto
 
# La foto se guarda junto a este archivo (sin rutas absolutas)
RUTA_FOTO = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                         "foto_capturada.jpg")
 
 
def recibir_exactamente(sock, cantidad):
    # Lee del socket hasta completar 'cantidad' bytes (recv puede devolver menos).
    datos = bytearray()
 
    while len(datos) < cantidad:
        faltan = cantidad - len(datos)
        bloque = sock.recv(min(4096, faltan))
 
        if not bloque:
            raise ConnectionError(
                f"Conexión cerrada: recibidos {len(datos)} de {cantidad} bytes."
            )
 
        datos.extend(bloque)
 
    return bytes(datos)
 
 
def request_photo(ip, port=PORT, ruta_destino=RUTA_FOTO):
    """
    Pide una foto a la camara en (ip, port) y la guarda en ruta_destino.
    Devuelve el tamano de la imagen en bytes. Lanza RuntimeError si algo falla.
    """
    etapa = "conexión"
 
    try:
        # create_connection aplica el timeout tambien a los recv posteriores
        with socket.create_connection((ip, port), timeout=TIMEOUT) as client:
            etapa = "envío del comando"
            client.sendall(b"TAKE_PHOTO")
 
            etapa = "recepción del encabezado"
            size_bytes = recibir_exactamente(client, 4)
            img_size = struct.unpack("!I", size_bytes)[0]   # 32 bits, big-endian
 
            if img_size <= 0 or img_size > TAMANO_MAXIMO:
                raise ValueError(f"Tamaño de imagen inesperado: {img_size} bytes.")
 
            etapa = "descarga de la imagen"
            img_data = recibir_exactamente(client, img_size)
 
    except Exception as e:
        raise RuntimeError(f"Error durante {etapa}: {e}") from e
 
    # Escritura atomica: primero archivo temporal, despues se reemplaza el definitivo
    ruta_tmp = ruta_destino + ".tmp"
    with open(ruta_tmp, "wb") as f:
        f.write(img_data)
    os.replace(ruta_tmp, ruta_destino)
 
    return len(img_data)
 
 
if __name__ == "__main__":
    # Prueba manual desde consola: busca la camara y pide una foto
    from Programacion.Interfaz.descubrimiento_camara import descubrir_camaras
 
    print("Buscando cámara en la red...")
    camaras = descubrir_camaras()
 
    if not camaras:
        print("No se encontró ninguna cámara.")
    else:
        ip, mac, puerto = camaras[0]
        print(f"Cámara {mac} en {ip}:{puerto}")
 
        try:
            n = request_photo(ip, puerto)
            print(f"¡Foto guardada! {n} bytes en {RUTA_FOTO}")
        except RuntimeError as e:
            print(e)