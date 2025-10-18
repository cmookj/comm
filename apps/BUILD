load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_binary(
    name = "client",
    srcs = ["client.cc"],
    deps = [
        "//core:comm",
    ],
)

cc_binary(
    name = "server",
    srcs = ["server.cc"],
    deps = [
        "//core:comm",
    ],
)
