from PIL import Image
from PIL import ImageEnhance
from PIL import ImageFilter

# Open the image
img = Image.open(r"c:/Users/tiago/OneDrive/Documentos/UTEC/PIC II/PIC-2_Sistema_clasificacion_pallets/Programacion/Procesamiento de Imagen/mate_de_prueba.jpg")

# Access basic properties
print(f"Format: {img.format}")
print(f"Mode: {img.mode}")
print(f"Size: {img.size}")
print(f"Width: {img.width} pixels")
print(f"Height: {img.height} pixels")

# Adjust brightness 
enhancer = ImageEnhance.Brightness(img)
brightened = enhancer.enhance(3)
brightened.save(r"c:/Users/tiago/OneDrive/Documentos/UTEC/PIC II/PIC-2_Sistema_clasificacion_pallets/Programacion/Procesamiento de Imagen/Edicion de imagenes/brightened.jpg")

# Adjust contrast
enhancer = ImageEnhance.Contrast(img)
contrasted = enhancer.enhance(3)
contrasted.save(r"c:/Users/tiago/OneDrive/Documentos/UTEC/PIC II/PIC-2_Sistema_clasificacion_pallets/Programacion/Procesamiento de Imagen/Edicion de imagenes/contrasted.jpg")

# Adjust color saturation
enhancer = ImageEnhance.Color(img)
saturated = enhancer.enhance(3)
saturated.save(r"c:/Users/tiago/OneDrive/Documentos/UTEC/PIC II/PIC-2_Sistema_clasificacion_pallets/Programacion/Procesamiento de Imagen/Edicion de imagenes/saturated.jpg")

# Apply sharpening
sharpened = img.filter(ImageFilter.SHARPEN)
sharpened.save(r"c:/Users/tiago/OneDrive/Documentos/UTEC/PIC II/PIC-2_Sistema_clasificacion_pallets/Programacion/Procesamiento de Imagen/Edicion de imagenes/sharpened.jpg")

# Apply Gaussian blur
blurred = img.filter(ImageFilter.GaussianBlur(radius=5))
blurred.save(r"c:/Users/tiago/OneDrive/Documentos/UTEC/PIC II/PIC-2_Sistema_clasificacion_pallets/Programacion/Procesamiento de Imagen/Edicion de imagenes/blurred.jpg")


