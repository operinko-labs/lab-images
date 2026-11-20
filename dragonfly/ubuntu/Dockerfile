FROM ghcr.io/dragonflydb/dragonfly:v2.1.0@sha256:94891bb22ce2fb0f7a5a3ff1bb3b2fbbc5830e2523293e47723d87fa3e5b1184

LABEL org.opencontainers.image.source="https://github.com/operinko-labs/lab-images" \
      org.opencontainers.image.description="Dragonfly with Ubuntu security updates applied"

# Upstream builds on ubuntu:22.04 and ships whatever package versions that
# base had at release time; apply the security updates published since.
RUN apt-get update \
    && apt-get -y upgrade --no-install-recommends \
    && rm -rf /var/lib/apt/lists/*
