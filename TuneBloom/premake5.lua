project "TuneBloom"
    language "C++"
    cppdialect "C++20"
    systemversion "latest"

    exceptionhandling "Off"
    rtti "Off"

    targetdir ("bin/%{prj.name}-%{cfg.platform}-%{cfg.buildcfg}/out")
    objdir ("bin/%{prj.name}-%{cfg.platform}-%{cfg.buildcfg}/int")
    debugdir "../workdir"

    links {
        "sead"
    }

    includedirs {
        "include",
        "include/imgui",
        "include/snd-ply",

        "vendor/sead/sead/include",
        "vendor/sead/sead/vendor/glad/include",
    }

    files {
        "src/**.cpp"
    }

    flags {
        "MultiProcessorCompile",
        "ShadowedVariables",
        --"FatalWarnings"
    }

    filter "system:windows"
        defines {
            "SEAD_PLATFORM_WINDOWS",
            "SEAD_USE_GL"
        }

    filter "configurations:Debug"
        kind "ConsoleApp"
        defines { "SEAD_TARGET_DEBUG" }
        runtime "Debug"
        symbols "on"
        optimize "off"

    filter "configurations:Release"
        kind "ConsoleApp"
        defines { "SEAD_TARGET_DEBUG" }
        runtime "Release"
        symbols "on"
        optimize "speed"
        flags { "LinkTimeOptimization" }

    filter "configurations:Dist"
        kind "WindowedApp"
        defines { "SEAD_TARGET_DEBUG", "NDEBUG" }
        runtime "Release"
        symbols "off"
        optimize "speed"
        flags { "LinkTimeOptimization" }

group "Dependencies"
    include "vendor/sead/sead"
group ""
