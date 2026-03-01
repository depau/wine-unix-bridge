FROM debian:13

ARG TARGETPLATFORM

RUN if [ "$TARGETPLATFORM" != "linux/amd64" ] && [ "$TARGETPLATFORM" != "linux/386" ]; then \
      echo "Unsupported platform: $TARGETPLATFORM"; \
      exit 1; \
    fi

RUN dpkg --add-architecture i386 && \
    apt-get update && \
    apt-get install --no-install-recommends -y build-essential mingw-w64 wine libwine-dev libc6-dev && \
    if [ "$TARGETPLATFORM" = "linux/amd64" ]; then \
      apt-get install -y --no-install-recommends wine64 wine64-tools; \
    else \
      apt-get install -y --no-install-recommends wine32 wine32-tools; \
    fi
