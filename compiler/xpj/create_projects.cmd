@echo off

set XPJ="xpj4.exe"

%XPJ% -v 1 -t VC12 -p WIN32 -x ConvexDecomposition.xpj
%XPJ% -v 1 -t VC12 -p WIN64 -x ConvexDecomposition.xpj

cd ..
@rem cd vc11win64
cd vc12win64

goto cleanExit

:pauseExit
pause

:cleanExit

