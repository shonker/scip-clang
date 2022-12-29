cc_library(
    name = "rapidjson",
    hdrs = glob(["include/rapidjson/**/*.h"]),
    defines = [
        "RAPIDJSON_HAS_STDSTRING",
        "RAPIDJSON_HAS_CXX11_RANGE_FOR",
        "RAPIDJSON_HAS_CXX11_RVALUE_REFS",
        "RAPIDJSON_HAS_CXX11_TYPETRAITS",
        # FIXME(def: add-enforce): Add ENFORCE similar to scip-ruby and use it
        # "RAPIDJSON_ASSERT(x)=ENFORCE(x)",
    ],
    includes = ["include"],
    linkstatic = 1,
    visibility = ["//visibility:public"],
)