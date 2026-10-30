FROM docker.io/nginxinc/nginx-unprivileged:1.31-alpine

LABEL org.opencontainers.image.source="https://github.com/operinko-labs/lab-images" \
      org.opencontainers.image.description="nginx-unprivileged with Alpine security updates applied"

# Upstream rebuilds only on nginx releases, so its Alpine packages lag
# security fixes (e.g. brotli-libs 1.2.1, curl 8.23.0); apply them here.
# apk needs root; drop back to the upstream unprivileged user afterwards.
USER root
RUN apk upgrade --no-cache
USER 101
