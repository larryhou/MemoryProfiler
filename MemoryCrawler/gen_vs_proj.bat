if "%1" == "-r" (
    rd /s /Q "build/Win"
)

if not exist "build/Win" (
    md "build/Win"
)

cd build/Win/ && call cmake ../../

PAUSE