FROM debian:stable
RUN apt-get update && apt-get install -y maven vim build-essential gdb tree
WORKDIR /build
COPY mylibrary mylibrary
COPY myapp myapp

WORKDIR /build/mylibrary
RUN mvn install
WORKDIR /build/myapp
RUN mvn compile
