# Demo: using the systemd directive OpenFile= with curl

The systemd directive [`OpenFile=`](https://www.freedesktop.org/software/systemd/man/systemd.service.html#OpenFile=) was introduced in __systemd 253__ (released 15 February 2023).

When using `OpenFile=`, the curl process does not need access privileges to the web server Unix socket because curl inherits the already connected socket as a file descriptor from its parent. This improves security because curl could then run with less privileges.

Build the container image

```
podman build -t demo docs/examples
```

Run a modified _externalsocket.c_ and download a web page from the Unix socket _$HOME/sockdir/sock_

```
systemd-run
  --quiet \
  --property OpenFile=$HOME/sockdir/sock:fdnametest \
  --user \
  --collect \
  --pipe \
  --wait \
  podman \
    run \
    --rm \
    --user 65534:65534 \
    localhost/demo fdnametest http://localhost
```

In the example, only `systemd-run` process need to have file access permissions to
_$HOME/sockdir/sock_

The program `/demo` does not need file access permissions to _$HOME/sockdir/sock_
