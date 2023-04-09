ARG fedoraversion=37
FROM registry.fedoraproject.org/fedora-minimal:${fedoraversion} AS appbuilder
RUN microdnf install -y gcc systemd-devel libcurl-devel
RUN mkdir /src
COPY externalsocket.c /src
RUN cd /src && gcc externalsocket.c -lsystemd -lcurl -o /demo

FROM registry.fedoraproject.org/fedora:${fedoraversion} AS osbuilder
ARG fedoraversion
RUN mkdir -p /target
RUN dnf install -y --installroot /target \
                   --releasever $fedoraversion \
                   --setopt install_weak_deps=false \
                   --nodocs \
                     coreutils-single \
                     glibc-minimal-langpack \
                     libcurl-minimal \
                     systemd-libs \
    && dnf --installroot /target clean all

FROM scratch
COPY --from=osbuilder /target /
COPY --from=appbuilder /demo /demo
ENTRYPOINT ["/demo"]
