# cv2pdb

This exists to convert the DWARF debug information to PDB and helps us
to get more precise stackstraces on program crashes.

Sadly this program is for windows only, but we can get it running using
WINE. You'll not only need wine and cv2pdb, but also Microsoft's PDB
helper libraries and dependencies.
Because of licensing issues I can't supply the needed files for you
here. What files work for me:

- cv2pdb.exe
- dbghelp.dll
- msobj140.dll
- mspdb140.dll
- mspdbcmf.exe
- mspdbcore.dll
- mspdbsrv.exe
- mspdbst.dll
- vcruntime140.dll

I found those libraries in my Visual Studio 2017 install:

    ..\2017\Community\VC\Tools\MSVC\14.11.25503\bin\Hostx86\x86

You can find cv2pdb binaries here:

    https://github.com/rainers/cv2pdb/releases

Apparently cv2pdb also supports older versions of mspdb:

- mspdb80
- mspdb100
- mspdb110
- mspdb120
- mspdb140
