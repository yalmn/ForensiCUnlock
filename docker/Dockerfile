# Offizielles Kali Linux Base-Image
FROM kalilinux/kali-rolling

# Projektmetadaten
LABEL org.opencontainers.image.title="ForensiCUnlock"
LABEL org.opencontainers.image.description="Tool zur automatisierten Entschlüsselung und Rekonstruktion von BitLocker-basierten BDP-Partitionen aus EWF- oder RAW-Images."
LABEL org.opencontainers.image.version="1.0"
LABEL org.opencontainers.image.authors="Dein Name <deine.mail@example.org>"

# Arbeitsverzeichnis innerhalb des Containers
WORKDIR /opt/forensicunlock

# Notwendige Tools installieren (für Kali typisch bereits verfügbar, hier zur Sicherheit explizit)
RUN apt update && apt install -y \
    build-essential \
    xmount \
    dislocker \
    ddrescue \
    sleuthkit \
    sudo \
    && apt clean

# Projektcode ins Container-Dateisystem kopieren
COPY . /opt/forensicunlock

# Projekt kompilieren
RUN make

# Einstiegspunkt: interaktive Bash-Shell
CMD ["bash"]
