# Run sample:
#   lib.ps1 -ARCH "Win32 x64" -CFG "Debug Release"

param ([string]$ARCH = "x64", [string]$CFG = "Release")
$vsroot=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
$msbuild=join-path $vsroot "MSBuild\Current\Bin\MSBuild.exe"
$lib="gsl"
${gsl-cmake-flags} = ""
foreach ($i in $lib)
{
    foreach ($j in $ARCH.Split(" ")) 
    {
        if (Test-Path $i-$j) { rm $i-$j -Force -Recurse }
        New-Item -ItemType Directory -Force -Path $i-$j
        cd $i-$j
        ${lib-flags} = (Get-Variable -Name ($lib + "-cmake-flags")).Value.Trim()
        cmake $(if (${lib-flags}) { ${lib-flags} + " " })-D CMAKE_C_FLAGS_INIT="/GL" -D CMAKE_STATIC_LINKER_FLAGS_INIT="/LTCG" -D CMAKE_GENERATOR_PLATFORM="$j" -G "Visual Studio 16" $(join-path .. $i)
        foreach ($k in $CFG.Split(" ")) { & $msbuild "$i.sln" /t:$i /p:Configuration=$k /p:CharacterSet=Unicode }
        cd ..
    }
}
