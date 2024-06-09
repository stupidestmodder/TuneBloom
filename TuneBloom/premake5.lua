project "TuneBloom"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    staticruntime "off"
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
        --"ShadowedVariables",
        --"FatalWarnings"
    }

    filter "system:windows"
        defines "SEAD_PLATFORM_WINDOWS"
        links "winmm.lib"

    filter "configurations:Debug"
        defines { "SEAD_DEBUG", "DEBUG" }
        runtime "Debug"
        symbols "on"
        optimize "off"

    filter "configurations:Release"
        defines { "SEAD_DEBUG", "NDEBUG" }
        runtime "Release"
        symbols "on"
        optimize "speed"
        flags { "LinkTimeOptimization" }

    filter "configurations:Dist"
        defines { "NDEBUG" }
        runtime "Release"
        symbols "off"
        optimize "speed"
        flags { "LinkTimeOptimization" }

group "Dependencies"
    include "vendor/sead/sead"
group ""
