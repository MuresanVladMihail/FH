# In order to run this test you must have:
# 1) make test_dynamic_lib which will create libcustom.so/dylib/dll
# 2) make install which will install fh and its headers
# By doing this you'll be able to execute dynamic loaded coded inside FH!

OS=$(uname -a | cut -d " " -f1 | awk '{ print $1 }')
echo "Running on OS: " $OS

if [[ $OS = "Darwin" ]] ;
then
    fh -l ./libcustom.dylib ./main.fh
fi
if [[$OS = "Linux"]] ;
then
    fh -l ./libcustom.so ./main.fh
fi
if [[ $OS = "OpenBSD" ]] ;
    fh -l ./libcustom.so ./main.fh
fi
