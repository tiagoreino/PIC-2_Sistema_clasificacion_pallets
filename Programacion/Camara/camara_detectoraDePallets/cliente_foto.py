import socket
import struct
from io import BytesIO

ESP32_IP = "192.168.1.10"
PORT = 5000
TIMEOUT = 30.0


def recibir_exactamente(sock, cantidad):
    datos = bytearray()

    while len(datos) < cantidad:
        faltan = cantidad - len(datos)
        bloque = sock.recv(min(4096, faltan))

        if not bloque:
            raise ConnectionError(
                f"Conexión cerrada: recibidos {len(datos)} "
                f"de {cantidad} bytes."
            )

        datos.extend(bloque)

    return bytes(datos)


def request_photo():
    client = None
    etapa = "conexión"

    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as client:
            client.settimeout(TIMEOUT)
            client.connect((ESP32_IP, PORT))
            print("Conectado a la ESP32-CAM.")

            etapa = "envío del comando"
            client.sendall(b"TAKE_PHOTO")

            etapa = "recepción del encabezado"
            size_bytes = recibir_exactamente(client, 4)
            img_size = struct.unpack("!I", size_bytes)[0]

            print(f"Tamaño anunciado por la ESP32: {img_size} bytes")

            if img_size <= 0 or img_size > 2_000_000:
                raise ValueError(
                    f"Tamaño de imagen inesperado: {img_size} bytes."
                )

            etapa = "descarga de la imagen"
            img_data = bytearray()

            while len(img_data) < img_size:
                faltan = img_size - len(img_data)
                bloque = client.recv(min(4096, faltan))

                if not bloque:
                    print(
                        f"Conexión cerrada: recibidos {len(img_data)} "
                        f"de {img_size} bytes."
                    )
                    return

                img_data.extend(bloque)
                print(f"Recibidos {len(img_data)} / {img_size} bytes")

            with open(
                r"C:\Users\tiago\OneDrive\Documentos\UTEC\PIC II"
                r"\PIC-2_Sistema_clasificacion_pallets\Programacion"
                r"\Camara\camara_detectoraDePallets\foto_capturada.jpg",
                "wb"
            ) as f:
                f.write(img_data)

            print(
                f"¡Foto guardada! {len(img_data)} bytes recibidos "
                "en foto_capturada.jpg"
            )

    except socket.timeout:
        print(f"Timeout durante la etapa: {etapa}.")
    except Exception as e:
        print(f"Error durante {etapa}: {e}")


if __name__ == "__main__":
    request_photo()
