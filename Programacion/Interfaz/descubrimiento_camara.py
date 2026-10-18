import socket
import time
 
DISCOVERY_PORT = 5001
DISCOVERY_REQUEST = b"DISCOVER_PALLET_CAM"
 
 
def descubrir_camaras(intentos=3, espera_por_intento=0.7):
    """
    Devuelve una lista de tuplas (ip, mac, puerto_tcp), una por camara que respondio.
    UDP no garantiza entrega, por eso se repite el pedido varias veces.
    """
    encontradas = {}
 
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        s.settimeout(0.2)
 
        for _ in range(intentos):
            s.sendto(DISCOVERY_REQUEST, ("255.255.255.255", DISCOVERY_PORT))
            fin = time.time() + espera_por_intento
 
            while time.time() < fin:
                try:
                    datos, (ip, _puerto_origen) = s.recvfrom(128)
                except socket.timeout:
                    continue
 
                partes = datos.decode(errors="replace").strip().split(";")
                if len(partes) != 3 or partes[0] != "PALLET_CAM":
                    continue  # paquete ajeno al protocolo
 
                try:
                    puerto_tcp = int(partes[2])
                except ValueError:
                    continue
 
                encontradas[ip] = (ip, partes[1], puerto_tcp)
 
    return list(encontradas.values())
 
 
def elegir_por_sufijo_mac(camaras, service_name):
    """
    Si hay mas de una camara en la red, elige la que corresponde al nombre de
    servicio BLE usado al provisionar (ej: 'PROV_720044' -> MAC termina en 72:00:44).
    Devuelve la tupla de la camara, o None si no hay coincidencia.
    """
    sufijo = service_name.replace("PROV_", "").upper()   # '720044'
    for ip, mac, puerto in camaras:
        if mac.replace(":", "").upper().endswith(sufijo):
            return (ip, mac, puerto)
    return None
 
 
if __name__ == "__main__":
    print("Buscando camaras en la red...")
    resultado = descubrir_camaras()
 
    if not resultado:
        print("No se encontro ninguna camara.")
    for ip, mac, puerto in resultado:
        print(f"Camara encontrada: IP {ip}  MAC {mac}  puerto TCP {puerto}")