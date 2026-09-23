import socket
import random

HOST = "0.0.0.0"
PORT = 5000

def main():
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((HOST, PORT))
    server.listen(1)
    print(f"Escuchando en {HOST}:{PORT}")

    while True:
        conn, addr = server.accept()
        print(f"Conexión de {addr}")
        with conn:
            data = conn.recv(1024)
            print(f"Recibido: {data}")

            signal = random.choice([b"0", b"1"])
            conn.sendall(signal)
            print(f"Enviado: {signal}")

if __name__ == "__main__":
    main()