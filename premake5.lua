-- premake5.lua
workspace "TuneBloom"
    configurations { "Debug", "Release", "Dist" }
    platforms { "Win32", "Win64" }
	toolset "clang"
    startproject "TuneBloom"
	
    filter { "toolset:clang" }
        staticruntime "on"
    	disablewarnings {
    		"invalid-offsetof",
    		"c++11-narrowing",
    	}

    filter { "platforms:Win32" }
        system "Windows"
        architecture "x86"
    
    filter { "platforms:Win64" }
        system "Windows"
        architecture "x86_64"
	
	filter { "configurations:Debug", "platforms:x64", "toolset:clang" }
    	sanitize {
    		"Address",
    		--"UndefinedBehavior",
    	}

include "TuneBloom"
