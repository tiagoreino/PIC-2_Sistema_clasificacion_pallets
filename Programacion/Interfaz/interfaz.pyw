from tkinter import *
from tkinter import ttk  
from PIL import Image, ImageTk 

# Función para habilitar o deshabilitar botones segun el modo
def cambiar_modo():
    if opcion_seleccionada.get() == 1:  # 1 es automatico
        # Botones de movimiento desactivados
        boton_avanzar.config(state="disabled")
        boton_detener.config(state="disabled")
        boton_retroceder.config(state="disabled")
        boton_subir.config(state="disabled")
        boton_bajar.config(state="disabled")
        # Selector de Pallet activado
        elegidor.config(state="readonly")
    else:  # 2 es manual
        # Botones de movimiento activados
        boton_avanzar.config(state="normal")
        boton_detener.config(state="normal")
        boton_retroceder.config(state="normal")
        boton_subir.config(state="normal")
        boton_bajar.config(state="normal")
        # Selector de Pallet desactivado
        elegidor.config(state="disabled")

# 1. Ventana Principal
raiz = Tk()
raiz.title("|| Sistema de Clasificación de Pallets ||")
raiz.iconbitmap(r"c:/Users/tiago/OneDrive/Documentos/UTEC/PIC II/PIC-2_Sistema_clasificacion_pallets/utecLogo.ico")
raiz.geometry("600x880") 
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

botonConectar = Button(conexion_frame, text="Conectar", bg="green", fg="white", width=15)
botonConectar.grid(row=3, column=1, padx=5, pady=10)

# 3. Frame para seleccion de pallets
conexion_pallet = Frame(raiz, bg="white", bd=5, relief="groove")
conexion_pallet.pack(fill="x", side="top", padx=10, pady=5)

palletLabel = Label(conexion_pallet, text="Clasificación:", bg="white", font=("Arial", 10, "bold"))
palletLabel.grid(row=0, column=0, padx=5, pady=5, sticky="w")

elegidor = ttk.Combobox(conexion_pallet, values=["Pallet 1", "Pallet 2", "Pallet 3"], state="readonly", width=25)
elegidor.grid(row=0, column=1, padx=5, pady=5)
elegidor.set("Selecciona tipo de Pallet") 

# Operaciones: Modo Manual 
modo_manual = Frame(raiz, bg="white", bd=5, relief="groove")
modo_manual.pack(fill="x", side="top", padx=10, pady=5)

cinta_label = Label(modo_manual, text="Cinta Transportadora:", bg="white", font=("Arial", 10, "bold"))
cinta_label.grid(row=0, column=0, columnspan=3, padx=5, pady=5, sticky="w")

boton_avanzar = Button(modo_manual, text="Avanzar", bg="blue", fg="white", width=15)
boton_avanzar.grid(row=1, column=0, padx=5, pady=5)

boton_detener = Button(modo_manual, text="Detener", bg="red", fg="white", width=15)
boton_detener.grid(row=1, column=1, padx=5, pady=5)

boton_retroceder = Button(modo_manual, text="Retroceder", bg="orange", fg="white", width=15)
boton_retroceder.grid(row=1, column=2, padx=5, pady=5)

piston_label = Label(modo_manual, text="Pistón:", bg="white", font=("Arial", 10, "bold"))
piston_label.grid(row=2, column=0, columnspan=3, padx=5, pady=5, sticky="w")

boton_subir = Button(modo_manual, text="Subir", bg="blue", fg="white", width=15)
boton_subir.grid(row=3, column=0, padx=5, pady=5)

boton_bajar = Button(modo_manual, text="Bajar", bg="red", fg="white", width=15)
boton_bajar.grid(row=3, column=1, padx=5, pady=5)

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

imagen_pil = Image.open(r"c:/Users/tiago/OneDrive/Documentos/UTEC/PIC II/PIC-2_Sistema_clasificacion_pallets/Programacion/Camara/wifi_camara_test/foto_capturada.jpg")
imagen_redimensionada = imagen_pil.resize((560, 260)) 
miImagen = ImageTk.PhotoImage(imagen_redimensionada)

Label(camara_frame, image=miImagen, bg="black").pack(expand=True)

raiz.mainloop()
