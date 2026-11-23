"""PlatformIO post-script for the native env: build only the C core of U8g2.

The library's C++ wrappers need Arduino; the host tests only use the C drawing core.
"""
Import("env")  # noqa: F821 -- provided by PlatformIO/SCons

if env["PIOPLATFORM"] == "native":  # noqa: F821
    for builder in env.GetLibBuilders():  # noqa: F821
        if builder.name == "U8g2":
            builder.env.AddBuildMiddleware(lambda env, node: None, "*.cpp")
