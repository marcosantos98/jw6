mkdir -Force .\build > $null

$INCLUDE_DIRS = @("./raylib/include")
$LIB_DIRS = @("./raylib/lib")
$RUNTIME_LIBS = @("kernel32", "msvcrt", "ucrt", "vcruntime", "msvcprt")
$LIBS = @("raylib", "user32", "gdi32", "winmm", "shell32")
$CPP_FLAGS = "-std=c++17 -ggdb"
$LINKER_FLAGS = @("/NODEFAULTLIB", "/IGNORE:4099")#, "/SUBSYSTEM:WINDOWS", "/entry:mainCRTStartup")

$clang_cmd = @(
	"clang++",
	$CPP_FLAGS,
	$(($INCLUDE_DIRS | ForEach-Object { "-I$_"}) -join ' '),
	$(($LIB_DIRS | ForEach-Object { "-L$_"}) -join ' '),
	$(($LIBS | ForEach-Object { "-l$_"}) -join ' '),
	$(($RUNTIME_LIBS | ForEach-Object { "-l$_"}) -join ' '),
	$(($LINKER_FLAGS | ForEach-Object { "-Xlinker $_"}) -join ' '),
	"-o ./build/main.exe",
	"main.cpp"
);

$cmd = $($clang_cmd -join ' ');

Write-Host "Exec: $cmd"

$took = Measure-Command {Invoke-Expression $cmd}
Write-Host "Timmings:" -ForegroundColor Green
Write-Host "    MilliSeconds: $($took.MilliSeconds)ms"
Write-Host "    Seconds: $($took.Seconds)s"

if ( $LastExitCode -ne 0) {
	Write-Host "Failed compilation..." -ForegroundColor Red
} 
