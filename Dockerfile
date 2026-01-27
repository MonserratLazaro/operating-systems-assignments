# Dockerfile para entorno de práctica en C (conceptos de Sistemas Operativos)
FROM ubuntu:22.04

ARG DEBIAN_FRONTEND=noninteractive

# Crear usuario no-root para trabajar (mejor seguridad/UX)
ENV USER=student
ENV UID=1000
ENV WORKDIR=/workspace

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    gdb \
    valgrind \
    strace \
    ltrace \
    man \
    ca-certificates \
    vim \
    less \
    pkg-config \
    iproute2 \
    sudo \
    python3 \
    python3-pip \
    libc6-dev \
    git \
 && rm -rf /var/lib/apt/lists/*

# Crear usuario
RUN useradd -m -u ${UID} -s /bin/bash ${USER} \
 && echo "${USER} ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/${USER} \
 && chmod 0440 /etc/sudoers.d/${USER}

WORKDIR ${WORKDIR}

# Default user
USER ${USER}

# Monta tu código desde host al /workspace (lo maneja docker-compose)
VOLUME ["/workspace"]

# Por defecto, mantiene el contenedor corriendo en background
CMD ["bash", "-lc", "echo 'Contenedor listo. Usa docker compose exec lab bash'; sleep infinity"]