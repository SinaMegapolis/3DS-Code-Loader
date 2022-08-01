# CRO and ExeFS loader+linker plugin for IDA Pro 7.6

This is an IDA plugin written in C++ that can, given a decrypted CXI file, load the entirety of the codebase into IDA and link CROs + code.bin together.
To use it simply download the Dll from Releases and put it inside <IDA>/loader folder.

## Environment Variables

For the source code to properly compile, you should have two environment variables on your OS:
-IDABIN: should point to the ida.exe file inside your IDA Pro installation
-IDASDK: should point to the root folder for your IDA SDK

###Notes

-This plugin has only been tested on Windows, with CXI files extracted from CIAs using ninfs.
-Since IDA doesn't support multiple file input, this plugin loads them in a hacky way by putting them on random addresses and treating everything as a single codebase. While this approach allows for better static analysis it also hampers debugging greatly.
-This plugin has only been tested with Pokemon XY and Pokemon ORAS, your mileage may vary.
-This plugin was made with RomFS IntegrityVerificationFileCheck version 1 in mind. Using it with games that use IVFC ver2 (games from the New3DS era) may not work.

