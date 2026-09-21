# Basis: Kali Linux mit allen Laufzeit-Abhängigkeiten
FROM kalilinux/kali-rolling AS base

LABEL org.opencontainers.image.title="ForensiCUnlock"
LABEL org.opencontainers.image.description="Entschlüsselt BitLocker-Partitionen in EWF- oder RAW-Images und setzt das vollständige Image wieder zusammen."
LABEL org.opencontainers.image.version="2.0.0"
LABEL org.opencontainers.image.authors="H. Yalman (https://github.com/yalmn)"
LABEL org.opencontainers.image.licenses="MIT"

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        build-essential \
        dislocker \
        ewf-tools \
        fuse3 \
        sleuthkit \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/forensicunlock

# Test-Umgebung: docker build --target test ...
FROM base AS test
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
        fdisk \
        xz-utils \
    && rm -rf /var/lib/apt/lists/*

# Laufzeit-Image (Standard)
FROM base AS runtime
COPY . /opt/forensicunlock
RUN make clean && make
ENTRYPOINT ["/opt/forensicunlock/forensic_unlock"]
