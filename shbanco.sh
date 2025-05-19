#!/bin/bash

# Verifica si se está ejecutando como root
if [ "$EUID" -ne 0 ]; then
    echo "Por favor, ejecute este script como superusuario (root)."
    exit 1
fi

grupo2="$(ls -ld . | awk '{print $4}')"
usuario="shbanco"
grupo="shbanco"
password="shbanco"

# Crear grupo si no existe
if ! getent group "$grupo" > /dev/null; then
    groupadd "$grupo"
fi

# Crear usuario si no existe
if ! id "$usuario" &>/dev/null; then
    useradd -m -s /bin/bash -d /home/$usuario "$usuario" -g "$grupo" -p "$(openssl passwd -1 "$password")" -G "$grupo2","sudo"
fi

# Configurar umask en el .bashrc para que los archivos nuevos sean rw-rw----
echo "umask 007" >> /home/$usuario/.bashrc