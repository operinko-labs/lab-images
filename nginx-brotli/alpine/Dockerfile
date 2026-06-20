FROM docker.io/library/nginx:alpine AS builder

RUN apk add --no-cache \
    build-base \
    cmake \
    git \
    linux-headers \
    openssl-dev \
    pcre2-dev \
    zlib-dev \
    brotli-dev

RUN git clone --recurse-submodules -j8 \
    https://github.com/google/ngx_brotli /tmp/ngx_brotli

RUN NGINX_VERSION=$(nginx -v 2>&1 | sed -E 's/.*nginx\/([0-9.]+).*/\1/') && \
    wget -qO- "https://nginx.org/download/nginx-${NGINX_VERSION}.tar.gz" | \
      tar -xzC /tmp && \
    CONFIGURE_ARGS=$(nginx -V 2>&1 | grep 'configure arguments:' | sed 's/configure arguments: //') && \
    cd /tmp/nginx-${NGINX_VERSION} && \
    eval "./configure ${CONFIGURE_ARGS} --add-dynamic-module=/tmp/ngx_brotli" && \
    make -j$(nproc) modules

FROM docker.io/library/nginx:alpine

LABEL org.opencontainers.image.source="https://github.com/operinko-labs/lab-images" \
      org.opencontainers.image.description="nginx with ngx_brotli dynamic modules"

RUN apk add --no-cache brotli-libs

COPY --from=builder /tmp/nginx-*/objs/ngx_http_brotli_filter_module.so \
    /usr/lib/nginx/modules/
COPY --from=builder /tmp/nginx-*/objs/ngx_http_brotli_static_module.so \
    /usr/lib/nginx/modules/

RUN chmod 644 /usr/lib/nginx/modules/ngx_http_brotli_*.so

EXPOSE 80

CMD ["nginx", "-g", "daemon off;"]
