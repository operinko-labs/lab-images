FROM php:8.5-fpm-alpine

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
    hiredis \
    zstd-libs \
    tini

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
        hiredis-dev \
        zstd-dev \
        imagemagick-dev \
        linux-headers \
    && docker-php-ext-configure gd --with-freetype --with-jpeg --with-webp \
    && docker-php-ext-install -j$(nproc) \
        gd pdo pdo_mysql zip intl mbstring bcmath exif \
    && pecl install igbinary \
    && pecl install redis \
    && pecl install zstd \
    && pecl install apcu \
    && pecl install uploadprogress \
    && pecl install imagick \
    && docker-php-ext-enable igbinary redis zstd apcu uploadprogress imagick \
    && mkdir -p /tmp/php-ext-brotli \
    && wget -qO- https://github.com/kjdev/php-ext-brotli/archive/refs/tags/0.19.0.tar.gz \
       | tar -xz --strip-components=1 -C /tmp/php-ext-brotli \
    && cd /tmp/php-ext-brotli && phpize && ./configure --enable-brotli --with-libbrotli \
    && make && make install \
    && rm -rf /tmp/php-ext-brotli \
    && docker-php-ext-enable brotli \
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

# Make drush (installed in vendor/bin) available globally
ENV PATH="$PATH:/var/www/html/vendor/bin"

RUN mkdir -p /var/www/html && chown -R www-data:www-data /var/www/html

WORKDIR /var/www/html
EXPOSE 9000
