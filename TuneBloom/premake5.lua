project "TuneBloom"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    systemversion "latest"

    exceptionhandling "Off"
    rtti "Off"

    targetdir ("bin/%{prj.name}-%{cfg.buildcfg}/out")
    objdir ("bin/%{prj.name}-%{cfg.buildcfg}/int")
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
        defines { "SEAD_TARGET_DEBUG" }
        runtime "Debug"
        symbols "on"
        optimize "off"

    filter "configurations:Release"
        defines { "SEAD_TARGET_DEBUG" }
        runtime "Release"
        symbols "on"
        optimize "speed"
        flags { "LinkTimeOptimization" }

    filter "configurations:Dist"
        defines { "NDEBUG" }
        runtime "Release"
        symbols "on"
        optimize "speed"
        flags { "LinkTimeOptimization" }

group "Dependencies"
    include "vendor/sead/sead"
group ""
