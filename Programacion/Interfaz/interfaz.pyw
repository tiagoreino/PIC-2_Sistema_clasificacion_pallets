from tkinter import *
from tkinter import ttk  
from tkinter import messagebox
from PIL import Image, ImageTk 
import os
import subprocess
import threading

SCRIPT_FOTO = (
    r"C:\Users\tiago\OneDrive\Documentos\UTEC\PIC II"
    r"\PIC-2_Sistema_clasificacion_pallets\Programacion"
    r"\Camara\camara_detectoraDePallets\cliente_foto.py"
)

PYTHON = r"C:\Espressif\tools\python\v6.1\venv\Scripts\python.exe"

def sacar_foto():
    if opcion_seleccionada.get() != 2:
        return

    botonFoto.config(state="disabled", text="Tomando foto...")

    def ejecutar():
        try:
            resultado = subprocess.run(
                [PYTHON, SCRIPT_FOTO],
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                timeout=60,
                creationflags=subprocess.CREATE_NO_WINDOW
            )

            if resultado.returncode != 0:
                detalle = resultado.stderr or resultado.stdout
                raiz.after(
                    0,
                    lambda error=detalle: messagebox.showerror(
                    "Error al sacar foto",
                    error or "El cliente terminó con error."
                    )
                )
                

        except Exception as error:
            raiz.after(
                0,
                lambda detalle=str(error): messagebox.showerror(
                    "Error al sacar foto",
                    detalle
                )
            )

        finally:
            raiz.after(0, restaurar_boton_foto)

    threading.Thread(target=ejecutar, daemon=True).start()


def restaurar_boton_foto():
    estado = (
        "normal"
        if opcion_seleccionada.get() == 2
        else "disabled"
    )
    botonFoto.config(state=estado, text="Sacar foto")


# Función para habilitar o deshabilitar botones segun el modo
def cambiar_modo():
    if opcion_seleccionada.get() == 1:  # Automático
        elegidor.config(state="readonly")
        botonFoto.config(state="disabled")
    else:  # Manual
        elegidor.config(state="disabled")
        botonFoto.config(state="normal")

ruta_foto = (
    r"c:/Users/tiago/OneDrive/Documentos/UTEC/PIC II/"
    r"PIC-2_Sistema_clasificacion_pallets/Programacion/Camara/"
    r"camara_detectoraDePallets/foto_capturada.jpg"
)

def actualizar_foto():
    try:
        with Image.open(ruta_foto) as imagen:
            imagen.load()
            imagen_redimensionada = imagen.resize((560, 260))

        foto_tk = ImageTk.PhotoImage(imagen_redimensionada)
        etiqueta_foto.config(image=foto_tk)
        etiqueta_foto.image = foto_tk

    except (FileNotFoundError, OSError):
        pass

    raiz.after(300, actualizar_foto)

# Rutas del cliente de provisionamiento
PYTHON_IDF = (
    r"C:\Espressif\tools\python\v6.1\venv\Scripts\python.exe"
)

SCRIPT_PROV = (
    r"C:\Users\tiago\Documents\idf-extra-components"
    r"\network_provisioning\tool\esp_prov\esp_prov.py"
)

IDF_PATH = r"C:\esp\v6.1\esp-idf"


def conectar_wifi():
    ssid = cuadroWifi.get().strip()
    password = cuadroContraseña.get()

    if not ssid:
        messagebox.showwarning(
            "Falta la red",
            "Ingresá el nombre de la red Wi-Fi."
        )
        return

    if not password:
        messagebox.showwarning(
            "Falta la contraseña",
            "Ingresá la contraseña del Wi-Fi."
        )
        return

    botonConectar.config(state="disabled", text="Conectando...")
    estadoConexion.config(
        text="Buscando la ESP32-CAM por Bluetooth...",
        fg="blue"
    )

    # El proceso se ejecuta en segundo plano para no
    # congelar la ventana de Tkinter.
    hilo = threading.Thread(
        target=ejecutar_provisionamiento,
        args=(ssid, password),
        daemon=True
    )
    hilo.start()


def ejecutar_provisionamiento(ssid, password):
    entorno = os.environ.copy()
    entorno["IDF_PATH"] = IDF_PATH

    comando = [
        PYTHON_IDF,
        SCRIPT_PROV,
        "--transport", "ble",
        "--service_name", "PROV_720044",
        "--sec_ver", "2",
        "--sec2_username", "CamaraPallets",
        "--sec2_pwd", "utec2026",
        "--ssid", ssid,
        "--passphrase", password
    ]

    try:
        resultado = subprocess.run(
            comando,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            env=entorno,
            timeout=180,
            creationflags=subprocess.CREATE_NO_WINDOW
        )

        salida = resultado.stdout + "\n" + resultado.stderr
        print("CÓDIGO DE SALIDA:", resultado.returncode)
        print("SALIDA DEL SCRIPT:")
        print(salida)

        if (
            resultado.returncode == 0
            and "Provisioning was successful" in salida
        ):
            raiz.after(0, finalizar_exito)
        else:
            detalle = salida[-2500:] or (
                f"El proceso terminó con código "
                f"{resultado.returncode}, sin más detalles."
            )
            raiz.after(
                0,
                lambda error=detalle: finalizar_error(error)
            )

    except subprocess.TimeoutExpired:
        raiz.after(
            0,
            lambda: finalizar_error(
                "Se agotó el tiempo de espera. "
                "Comprobá que la ESP32 esté anunciándose "
                "por BLE y que Bluetooth esté activado."
            )
        )

    except Exception as error:
        raiz.after(
            0,
            lambda detalle=str(error): finalizar_error(detalle)
        )


def finalizar_exito():
    estadoConexion.config(
        text="¡Wi-Fi configurado correctamente!",
        fg="green"
    )
    botonConectar.config(state="normal", text="Conectar")


    messagebox.showinfo(
        "Conexión exitosa",
        "La ESP32-CAM recibió las credenciales y "
        "se conectó a la red Wi-Fi."
    )


def finalizar_error(detalle):
    estadoConexion.config(
        text="No se pudo completar la conexión",
        fg="red"
    )
    botonConectar.config(state="normal", text="Conectar")

    messagebox.showerror(
        "Error de conexión",
        "No se pudo completar el provisionamiento.\n\n"
        + detalle
    )

# 1. Ventana Principal
raiz = Tk()
raiz.title("|| Sistema de Clasificación de Pallets ||")
raiz.iconbitmap(r"c:/Users/tiago/OneDrive/Documentos/UTEC/PIC II/PIC-2_Sistema_clasificacion_pallets/utecLogo.ico")
raiz.geometry("600x700") 
raiz.resizable(0, 0)
raiz.config(bg="white")

# 2. Frame para conexion
conexion_frame = Frame(raiz, bg="white", bd=5, relief="groove")
conexion_frame.pack(fill="x", side="top", padx=10, pady=5)

tituloIpLabel = Label(conexion_frame, text="Ip de la Esp32-CAM:", bg="white", font=("Arial", 10, "bold"))
tituloIpLabel.grid(row=0, column=0, padx=5, pady=5, sticky="w")

ipAsignadaLabel = Label(conexion_frame, text="(Ejemplo de IP: 192.168.1.100)", bg="white", fg="gray")
ipAsignadaLabel.grid(row=0, column=1, padx=5, pady=5, sticky="w")

wifiLabel = Label(conexion_frame, text="Ingresa Wifi:", bg="white")
wifiLabel.grid(row=1, column=0, padx=5, pady=5, sticky="w")

cuadroWifi = Entry(conexion_frame, width=30)
cuadroWifi.grid(row=1, column=1, padx=5, pady=5, sticky="w")

contraseñaLabel = Label(conexion_frame, text="Ingresa Contraseña:", bg="white")
contraseñaLabel.grid(row=2, column=0, padx=5, pady=5, sticky="w")

cuadroContraseña = Entry(conexion_frame, show="*", width=30)
cuadroContraseña.grid(row=2, column=1, padx=5, pady=5, sticky="w")

botonConectar = Button(
    conexion_frame,
    text="Conectar",
    bg="green",
    fg="white",
    width=15,
    command=conectar_wifi
)
botonConectar.grid(row=3, column=1, padx=5, pady=10)

estadoConexion = Label(
    conexion_frame,
    text="Esperando conexión",
    bg="white",
    fg="gray"
)
estadoConexion.grid(row=4, column=0, columnspan=2, padx=5, pady=5)
# 3. Frame para seleccion de pallets
conexion_pallet = Frame(raiz, bg="white", bd=5, relief="groove")
conexion_pallet.pack(fill="x", side="top", padx=10, pady=5)



palletLabel = Label(conexion_pallet, text="Clasificación:", bg="white", font=("Arial", 10, "bold"))
palletLabel.grid(row=0, column=0, padx=5, pady=5, sticky="w")

elegidor = ttk.Combobox(conexion_pallet, values=["Pallet 1", "Pallet 2", "Pallet 3"], state="readonly", width=25)
elegidor.grid(row=0, column=1, padx=5, pady=5)
elegidor.set("Selecciona tipo de Pallet") 

botonFoto = Button(
    conexion_pallet,
    text="Sacar foto",
    bg="blue",
    fg="white",
    width=15,
    command=sacar_foto,
    state="disabled"
)
botonFoto.grid(row=1, column=1, padx=5, pady=5)

# Modos de Opercion
modo_operacion = Frame(raiz, bg="white", bd=5, relief="groove")
modo_operacion.pack(fill="x", side="top", padx=10, pady=5)

opcion_seleccionada = IntVar()
opcion_seleccionada.set(1) 

modoLabel = Label(modo_operacion, text="Modo de Operación:", bg="white", font=("Arial", 10, "bold"))
modoLabel.grid(row=0, column=0, padx=5, pady=5, sticky="w")

radio_automatico = Radiobutton(modo_operacion, text="Automático", variable=opcion_seleccionada, value=1, bg="white", command=cambiar_modo)
radio_automatico.grid(row=0, column=1, padx=20, pady=5, sticky="w")

radio_manual = Radiobutton(modo_operacion, text="Manual", variable=opcion_seleccionada, value=2, bg="white", command=cambiar_modo)
radio_manual.grid(row=0, column=2, padx=20, pady=5, sticky="w")

# Frame para mostrar el resultado y el estado de la operacion
modo_resultado = Frame(raiz, bg="white", bd=5, relief="groove")
modo_resultado.pack(fill="x", side="bottom", padx=10, pady=5)

# Configuramos las columnas para que distribuyan el peso uniformemente
modo_resultado.grid_columnconfigure(0, weight=1)
modo_resultado.grid_columnconfigure(1, weight=1)

# Agrupamos Resultado y Estado en un sub-frame interno para centrarlos juntos
texto_centrado_frame = Frame(modo_resultado, bg="white")
texto_centrado_frame.grid(row=0, column=0, columnspan=2, pady=5)

modoResultado = Label(texto_centrado_frame, text="Resultado: ", bg="white", font=("Arial", 14, "bold"))
modoResultado.pack(side="left")

modoEstado = Label(texto_centrado_frame, text="Aceptado", bg="white", fg="green", font=("Arial", 14, "bold"))
modoEstado.pack(side="left")

# Ubicacion de Conformes y No Conformes
modoConformes = Label(modo_resultado, text="Conformes: 0", bg="lightgreen", font=("Arial", 10, "bold"), padx=5)
modoConformes.grid(row=1, column=0, padx=20, pady=5)

modoNoConformes = Label(modo_resultado, text="No Conformes: 0", bg="#ff9999", fg="black", font=("Arial", 10, "bold"), padx=5)
modoNoConformes.grid(row=1, column=1, padx=20, pady=5)


# Llamamos a cambiar_modo() para establecer el estado inicial de los botones
cambiar_modo()


# 4. Frame para mostrar la imagen de la camara
camara_frame = Frame(raiz, bg="black", bd=5, relief="solid")
camara_frame.pack(fill="both", expand=True, padx=10, pady=10)



imagen_pil = Image.open(ruta_foto)
imagen_redimensionada = imagen_pil.resize((560, 260))
miImagen = ImageTk.PhotoImage(imagen_redimensionada)

etiqueta_foto = Label(camara_frame, image=miImagen, bg="black")
etiqueta_foto.image = miImagen
etiqueta_foto.pack(expand=True)

actualizar_foto()
raiz.mainloop()