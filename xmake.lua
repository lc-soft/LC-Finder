set_project("lcfinder")
set_version("0.10.0")
set_warnings("all")
set_config_h("include/config.h")
add_cfuncs("3rdparty", "sqlite3", "sqlite3.h", "sqlite3_open")
add_includedirs("include", ".repos/LCUI/include")
add_ldflags(".repos/LCUI/src/.libs/libLCUI.a")
add_ldflags("-L.repos/LCUI/src/.libs")
add_ldflags("$(shell pkg-config .repos/LCUI/lcui.pc --static --libs-only-l)")

if is_mode("debug") then
    set_symbols("debug")
    set_optimize("none")
end

if is_mode("release") then
    set_symbols("hidden")
    set_strip("all")
end

target("lcfinder")
    set_kind("binary")
    add_files("src/**.c")

