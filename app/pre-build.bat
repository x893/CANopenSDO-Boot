@echo off

echo #ifndef COMMIT_ID>src/version.h
for /F %%i in ('git rev-parse --short HEAD') do echo #define COMMIT_ID 0x%%i>>src/version.h
echo #ifndef COMMIT_ID>>src/version.h
echo #error "COMMIT_ID Not define">>src/version.h
echo #define COMMIT_ID 000000>>src/version.h
echo #endif>>src/version.h
echo #endif>>src/version.h
