load("@rules_cc//cc:cc_library.bzl", "cc_library")

# cc_library(
#     name = "comm",
#     srcs = glob(["*.cc"]),
#     hdrs = glob(["*.h"]),
#     # deps = ["@boost//:boost"],
#     # Or, specify libraries
#     # deps = [
#     #     "@boost//:filesystem",
#     #     "@boost//:system",
#     # ],
#     deps = [
#         "@boost//:asio",
#     ],
#     visibility = ["//test:__pkg__"],
# )

cc_library(
    name = "comm",
    hdrs = glob(["*.h"]),
    includes = ["."],
    deps = [
        "@boost//:endian",
        "@boost//:asio_ssl",
    ],
    visibility = ["//visibility:public"],
)
