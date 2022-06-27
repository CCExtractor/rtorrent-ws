load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "uSockets",
    srcs = glob(["src/**/*.c",]),
    hdrs = glob(["src/**/*.h",]),
    copts = [
        "-std=c17",
        "-flto",
        "-O3",
        "-DLIBUS_NO_SSL"
    ],
    strip_include_prefix = "src",
    visibility = ["//visibility:public"]
)