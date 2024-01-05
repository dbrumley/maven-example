FROM debian:stable as builder1
RUN apt-get update && apt-get install -y maven vim build-essential gdb tree

WORKDIR /build

COPY myapp myapp
COPY mylibrary mylibrary
COPY myharness myharness
COPY pom.xml .

RUN mvn install

FROM debian:stable
RUN apt-get update && \
    apt-get install -y build-essential gdb && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*
COPY --from=builder1 /build/myharness/target/nar/myharness-1.0-SNAPSHOT-amd64-Linux-gpp-executable/bin/amd64-Linux-gpp/myharness /myharness