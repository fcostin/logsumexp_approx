FROM debian:sid-slim

# need debian slim for clang-13
RUN apt-get update -y && apt-get install -y clang-13 python3 make vim && rm -rf /var/lib/apt/lists/*

RUN mkdir -p /work
RUN adduser --quiet --disabled-password bot && chown -R bot /work
USER bot

WORKDIR /work


