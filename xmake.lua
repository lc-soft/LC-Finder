set_project("lcfinder")
set_version("0.10.0")
set_warnings("all")
set_config_h("include/config.h")
add_cfuncs("3rdparty", "LCUI", nil, "LCUI_Init")
add_cfuncs("3rdparty", "sqlite3", "sqlite3.h", "sqlite3_open")

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
    add_includedirs("include")