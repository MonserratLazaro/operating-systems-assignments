# Instrucciones de build y ejecución del contenedor

Crea una carpeta (por ejemplo os-lab) y dentro copia los archivos que vienen abajo.

Ejecuta:
```
docker compose up --build -d
```

Entra al contenedor:
```
docker compose exec lab bash
```

# Compilar código

Entra al contenedor y ejecuta:
```
gcc -o build/cualquier_nombre src/tu_archivo_c.c
```

# Ejecutar código

Entra al contenedor y ejecuta:
```
./build/nombre_de_tu_ejecutable
```

# Modificar o crear archivos

Entra al contenedor y ejecuta:
```
vim ruta/del/archivo/archivo.extension
```

Comandos básicos (Primero aprieta ctrl + c):

- i → entra en modo insertar (puedes escribir texto)
- Esc → salir del modo insertar
- :w → guardar
- :q → salir
- :wq → guardar y salir
- :q! → salir sin guardar