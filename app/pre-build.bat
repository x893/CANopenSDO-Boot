@echo off

echo #ifndef COMMIT_ID>core/version.h
for /F %%i in ('git rev-parse --short HEAD') do echo #define COMMIT_ID 0x%%i>>core/version.h
echo #ifndef COMMIT_ID>>core/version.h
echo #error "COMMIT_ID Not define">>core/version.h
echo #define COMMIT_ID 000000>>core/version.h
echo #endif>>core/version.h
echo #endif>>core/version.h
