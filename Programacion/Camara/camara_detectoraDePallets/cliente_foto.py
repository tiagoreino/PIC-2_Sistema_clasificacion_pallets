import socket
import struct

ESP32_IP = "192.168.1.8"  
PORT = 5000

def request_photo():
    try:
        client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        # Seteamos un timeout de 10 segundos para que no se quede colgado eternamente
        client.settimeout(10.0) 
        
        client.connect((ESP32_IP, PORT))
        print("Conectado a la ESP32-CAM.")
        
        # Enviar comando para tomar la foto
        client.sendall(b"TAKE_PHOTO")
        
        # Leer los primeros 4 bytes que contienen el tamaño de la imagen
        size_bytes = client.recv(4)
        if not size_bytes or len(size_bytes) < 4:
            print("No se recibió el encabezado de tamaño correcto.")
            return
        
        # CORRECCIÓN: Accedemos al índice [0] de la tupla unpack para tener el ENTERO puro
        img_size = struct.unpack("!I", size_bytes)[0]
        print(f"Descargando foto... Tamaño esperado: {img_size} bytes")
        
        # Leer el buffer de la imagen por fragmentos
        img_data = b""
        while len(img_data) < img_size:
            # Le pedimos dinámicamente solo lo que falta para no quedarnos bloqueados
            remaining = img_size - len(img_data)
            packet = client.recv(min(4096, remaining))
            if not packet:
                print("La conexión se cerró inesperadamente durante la descarga.")
                break
            img_data += packet
            
        if len(img_data) == img_size:
            # Guardar el archivo en el disco
            with open("foto_capturada.jpg", "wb") as f:
                f.write(img_data)
            print("¡Foto recibida y guardada exitosamente como 'foto_capturada.jpg'!")
        else:
            print(f"Error: Se recibieron {len(img_data)} bytes de los {img_size} esperados.")
        
    except socket.timeout:
        print("Error: Tiempo de espera agotado (Timeout) al descargar los datos.")
    except Exception as e:
        print(f"Error en la comunicación: {e}")
    finally:
        client.close()

if __name__ == "__main__":
    request_photo()