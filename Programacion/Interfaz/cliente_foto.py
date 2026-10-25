import os
import socket
import struct
import threading

PORT = 5000
TIMEOUT = 30.0
TIMEOUT_ESTADO = 5.0
TAMANO_MAXIMO = 2_000_000   # bytes; mas que esto se considera un encabezado corrupto

_AQUI = os.path.dirname(os.path.abspath(__file__))
RUTA_FOTO = os.path.join(_AQUI, "foto_capturada.jpg")            # foto cruda de la camara
RUTA_FOTO_ANALIZADA = os.path.join(_AQUI, "foto_analizada.jpg")  # salida de identificacion.py

# Codigo binario que la camara saca por sus dos pines hacia el Arduino
ESTADO_NADA = 0        # 00
ESTADO_ACEPTADO = 1    # 01
ESTADO_RECHAZADO = 2   # 10
ESTADO_MANUAL = 3      # 11

# El servidor TCP de la camara atiende de a un cliente: se serializan los pedidos
_candado_camara = threading.Lock()


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
        with _candado_camara:
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


def set_state(ip, estado, port=PORT):
    """
    Le pide a la camara que ponga 'estado' (0..3) en sus dos pines hacia el Arduino.
    Lanza RuntimeError si algo falla o la camara no confirma con OK.
    """
    if estado not in (ESTADO_NADA, ESTADO_ACEPTADO, ESTADO_RECHAZADO, ESTADO_MANUAL):
        raise ValueError(f"Estado inválido: {estado}")

    etapa = "conexión"
    respuesta = b""

    try:
        with _candado_camara:
            with socket.create_connection((ip, port), timeout=TIMEOUT_ESTADO) as client:
                etapa = "envío del estado"
                client.sendall(f"SET_STATE:{estado}".encode())

                etapa = "respuesta de la cámara"
                while True:   # la camara cierra el socket despues de responder
                    bloque = client.recv(16)
                    if not bloque:
                        break
                    respuesta += bloque
    except Exception as e:
        raise RuntimeError(f"Error durante {etapa}: {e}") from e

    if respuesta.strip() != b"OK":
        raise RuntimeError(f"La cámara respondió {respuesta!r} al estado {estado}")


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