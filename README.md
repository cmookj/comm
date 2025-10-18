# Messaging Server & Client

A header-only library for TCP/IP communication client & server.

## Dependency

* Boost ASIO
* OpenSSL

## Examples

Example client and server are in `apps` directory.

Build:

```shell
bazel build --compilation_mode="dbg" --cxxopt=-std=c++17 //apps:client
bazel build --compilation_mode="dbg" --cxxopt=-std=c++17 //apps:server
```
