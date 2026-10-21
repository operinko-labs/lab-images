FROM php:8.5.12-fpm-alpine

LABEL org.opencontainers.image.source="https://github.com/operinko-labs/lab-images" \
      org.opencontainers.image.description="Drupal PHP-FPM base image"

RUN apk add --no-cache \
    bash \
    mariadb-client \
    imagemagick \
    imagemagick-libs \
    libpng \
    libjpeg-turbo \
    libwebp \
    freetype \
    icu-libs \
    libzip \
    oniguruma \
    brotli-libs \
    libavif \
    hiredis \
    zstd-libs \
    tini

# netsyslog: PHP's syslog() over UDP instead of /dev/log (see netsyslog/).
COPY netsyslog /usr/src/netsyslog

RUN apk add --no-cache --virtual .build-deps \
        $PHPIZE_DEPS \
        libpng-dev \
        libjpeg-turbo-dev \
        libwebp-dev \
        libzip-dev \
        freetype-dev \
        icu-dev \
        oniguruma-dev \
        brotli-dev \
        libavif-dev \
        hiredis-dev \
        zstd-dev \
        imagemagick-dev \
        linux-headers \
    && docker-php-ext-configure gd --with-freetype --with-jpeg --with-webp --with-avif \
    && docker-php-ext-install -j$(nproc) \
        gd pdo pdo_mysql zip intl mbstring bcmath exif \
    # PECL extensions from their GitHub release tags, not pecl.php.net: PECL
    # went down under load on PHP2Shell release day (2026-10-21) and failed
    # the build that carried the fix. zstd links the system libzstd (the
    # GitHub archive has no bundled copy); imagick's archive leaves its
    # version as a placeholder that the PECL packager normally fills in.
    && for src in \
         igbinary@https://github.com/igbinary/igbinary/archive/refs/tags/3.2.17RC1.tar.gz \
         redis@https://github.com/phpredis/phpredis/archive/refs/tags/6.3.0.tar.gz \
         zstd@https://github.com/kjdev/php-ext-zstd/archive/refs/tags/0.18.0.tar.gz \
         apcu@https://github.com/krakjoe/apcu/archive/refs/tags/v5.1.28.tar.gz \
         uploadprogress@https://github.com/php/pecl-php-uploadprogress/archive/refs/tags/uploadprogress-2.0.2.tar.gz \
         imagick@https://github.com/Imagick/imagick/archive/refs/tags/3.8.1.tar.gz; do \
         name=${src%%@*}; \
         mkdir -p /tmp/ext/$name \
         && wget -qO- "${src#*@}" | tar -xz --strip-components=1 -C /tmp/ext/$name \
         && cd /tmp/ext/$name \
         && if [ $name = imagick ]; then sed -i 's/@PACKAGE_VERSION@/3.8.1/' php_imagick.h; fi \
         && phpize \
         && if [ $name = zstd ]; then ./configure --with-libzstd; else ./configure; fi \
         && make -j$(nproc) && make install \
         || exit 1; \
       done \
    && rm -rf /tmp/ext \
    && docker-php-ext-enable igbinary redis zstd apcu uploadprogress imagick \
    && mkdir -p /tmp/php-ext-brotli \
    && wget -qO- https://github.com/kjdev/php-ext-brotli/archive/refs/tags/0.19.0.tar.gz \
       | tar -xz --strip-components=1 -C /tmp/php-ext-brotli \
    && cd /tmp/php-ext-brotli && phpize && ./configure --enable-brotli --with-libbrotli \
    && make && make install \
    && rm -rf /tmp/php-ext-brotli \
    && docker-php-ext-enable brotli \
    && cd /usr/src/netsyslog && phpize && ./configure --enable-netsyslog \
    && make && make install \
    && docker-php-ext-enable netsyslog \
    && cd / && rm -rf /usr/src/netsyslog \
    && apk del .build-deps \
    && rm -rf /tmp/pear /var/cache/apk/*

# GNU Iconv – musl libc iconv is incomplete; use glibc's via preload
RUN apk add gnu-libiconv --update-cache \
    --repository http://dl-cdn.alpinelinux.org/alpine/edge/community/ \
    --allow-untrusted
ENV LD_PRELOAD="/usr/lib/preloadable_libiconv.so php"

# Configure php-fpm to listen on Unix socket for nginx sidecar via emptyDir
RUN sed -i \
      -e 's|listen = 127.0.0.1:9000|listen = /var/run/php/php-fpm.sock|' \
      -e 's|;listen.owner = www-data|listen.owner = www-data|' \
      -e 's|;listen.group = www-data|listen.group = www-data|' \
      -e 's|;listen.mode = 0660|listen.mode = 0666|' \
      /usr/local/etc/php-fpm.d/www.conf

# netsyslog is off until NETSYSLOG_HOST names a syslog receiver; unset, the
# stock syslog() writes to /dev/log as before. NETSYSLOG_HOSTNAME is the
# HOSTNAME field of each line (default: the container's hostname).
RUN printf '%s\n' \
      'netsyslog.host = ${NETSYSLOG_HOST}' \
      'netsyslog.hostname = ${NETSYSLOG_HOSTNAME}' \
      > /usr/local/etc/php/conf.d/zz-netsyslog.ini

# Make drush (installed in vendor/bin) available globally
ENV PATH="$PATH:/var/www/html/vendor/bin"

RUN mkdir -p /var/www/html && chown -R www-data:www-data /var/www/html

WORKDIR /var/www/html
EXPOSE 9000
