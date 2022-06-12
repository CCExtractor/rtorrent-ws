load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "uWebSockets",
    hdrs = glob(["src/*.h"]),
    strip_include_prefix = "src",
    include_prefix = "uWebSockets",
    visibility = ["//visibility:public"],
)