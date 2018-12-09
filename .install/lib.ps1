# Run sample:
#   lib.ps1 -ARCH "Win32 x64" -CFG "Debug Release"

param ([string]$ARCH = "x64", [string]$CFG = "Release")
$msbuild="C:\Program Files (x86)\Microsoft Visual Studio\2017\Enterprise\MSBuild\15.0\Bin\$ENV:PROCESSOR_ARCHITECTURE\MSBuild.exe"
$lib="gsl"
foreach ($i in $lib)
{
    foreach ($j in $ARCH.Split(" ")) 
    {
        if (Test-Path $i-$j) { rm $i-$j -Force -Recurse }
        New-Item -ItemType Directory -Force -Path $i-$j
        cd $i-$j
        cmake -D CMAKE_C_FLAGS_INIT="/GL" -D CMAKE_STATIC_LINKER_FLAGS_INIT="/LTCG" -D CMAKE_GENERATOR_PLATFORM=$j -G "Visual Studio 15" ../$i
        foreach ($k in $CFG.Split(" ")) { & $msbuild "${i}.sln" /t:$i /p:Configuration=$k }
        cd ..
    }
}
