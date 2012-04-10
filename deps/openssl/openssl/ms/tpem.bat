rem called by testpem

echo test %1 %2
%ssleay% %1 -in %2 -out %tmp1%
%cmp% %2 %tmp1%

