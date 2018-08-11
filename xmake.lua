set_project("lc-finder")
set_version("0.11.0")
set_warnings("all")
set_config_header("include/config.h", {prefix = "LCFINDER"})
add_cfuncs("3rdparty", "sqlite3", "sqlite3.h", "sqlite3_open")
add_cflags("$(shell pkg-config .repos/LCUI/lcui.pc --static --cflags-only-other)")
add_ldflags("-L.repos/LCUI/src/.libs")
add_ldflags("-L.repos/libyaml/src/.libs")
add_ldflags("-L.repos/lcui.css/dist/lib")
add_ldflags(".repos/LCUI/src/.libs/libLCUI.a -lpthread")
add_ldflags(".repos/lcui.css/dist/lib/libLCUIEx.a")
add_ldflags("$(shell pkg-config .repos/LCUI/lcui.pc --static --libs-only-l)")
add_ldflags("$(shell pkg-config .repos/libyaml/yaml-0.1.pc --static --libs-only-l)")
add_includedirs("include", ".repos/LCUI/include", ".repos/libyaml/include", ".repos/lcui.css/include")

if is_mode("debug") then
    set_symbols("debug")
    set_optimize("none")
end

if is_mode("release") then
    set_symbols("hidden")
    set_strip("all")
end

target("lc-finder")
    set_targetdir("app/")
    set_kind("binary")
    add_files("src/**.c")
