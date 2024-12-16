em++ -sMEMORY64=1 -msimd128 -O3 -fno-rtti -fno-exceptions -Wno-experimental -c llama-run.cpp -o llama-run.o -I/llamacpp/ -I/llamacpp/common -I/lua-5.3.4/src
emcc -sMEMORY64=1 -msimd128 -O3 -fno-rtti -fno-exceptions -Wno-experimental -c llama-bindings.c -o llama-bindings.o -I/llamacpp/ -I/llamacpp/common -I/lua-5.3.4/src

emar rcs libaollama.so llama-bindings.o llama-run.o

rm llama-bindings.o  llama-run.o


emcc stream-bindings.c -c -sMEMORY64=1 -msimd128 -O3 -fno-rtti -fno-exceptions -Wno-experimental  -o stream-bindings.o  -I/lua-5.3.4/src
emcc stream.c -c -sMEMORY64=1 -msimd128 -O3 -fno-rtti -fno-exceptions -Wno-experimental  -o stream.o -I/lua-5.3.4/src

emar rcs libaostream.so stream-bindings.o stream.o

rm stream.o stream-bindings.o
