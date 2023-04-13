# pltfall

Obfuscate library function calls by messing with PLT/GOT/RELA.

Supports only ELF x86-64 binary.

## What is this?

pltfall is a tool that can be used to copy (or swap) a library function's PLT/GOT/RELA values from corresponding one for another library function. 

Doing this will result in function calls behaving weirdly and tools (e.g. disassembler and ltrace) showing incorrect library function call.

## Demo

The following code (`example/example.c`) will be used for demonstration:
```c
#include <string.h>

int main(int argc, char *argv[])
{
        char buf1[3] = "A\0B";
        char buf2[sizeof(buf1)] = "A\0\0";

        char *func_ret[] = {"memcmp", "strncmp"};
        int ret = 0;

        if (argc > 1) {
                ret = strncmp(buf1, buf2, sizeof(buf1));
                printf("return value of strncmp(buf1, buf2, %ld): %02x (actual function called: %s)\n", sizeof(buf1), ret, func_ret[ret == 0]);
        }

	ret = memcmp(buf1, buf2, sizeof(buf1));
        printf("return value of memcmp(buf1, buf2, %ld):  %02x (actual function called: %s)\n", sizeof(buf1), ret, func_ret[ret == 0]);

        ret = strncmp(buf1, buf2, sizeof(buf1));
        printf("return value of strncmp(buf1, buf2, %ld): %02x (actual function called: %s)\n", sizeof(buf1), ret, func_ret[ret == 0]);

        ret = memcmp(buf1, buf2, sizeof(buf1));
        printf("return value of memcmp(buf1, buf2, %ld):  %02x (actual function called: %s)\n", sizeof(buf1), ret, func_ret[ret == 0]);

        if (argc <= 1) {
                ret = strncmp(buf1, buf2, sizeof(buf1));
                printf("return value of strncmp(buf1, buf2, %ld): %02x (actual function called: %s)\n", sizeof(buf1), ret, func_ret[ret == 0]);
        }

        return 0;
}
```

The program compares two buffers (`buf1 = "A\0B"` and `"buf2 = "A\0\0"`) using memcmp and strncmp:
```
$ ./example/example
return value of memcmp(buf1, buf2, 3):  42 (actual function called: memcmp)
return value of strncmp(buf1, buf2, 3): 00 (actual function called: strncmp)
return value of memcmp(buf1, buf2, 3):  42 (actual function called: memcmp)
return value of strncmp(buf1, buf2, 3): 00 (actual function called: strncmp)
```

Swapping both function's plt-got/rela-info value and running the program will still give the same result:
```
$ ./pltfall example/example example/example.plt-got.rela-info.0-swap.plt; mv example/example.obf example/example.plt-got.rela-info.0-swap.obf
bla bla bla...

$ ./example/example.plt-got.rela-info.0-swap.obf
return value of memcmp(buf1, buf2, 3):  42 (actual function called: memcmp)
return value of strncmp(buf1, buf2, 3): 00 (actual function called: strncmp)
return value of memcmp(buf1, buf2, 3):  42 (actual function called: memcmp)
return value of strncmp(buf1, buf2, 3): 00 (actual function called: strncmp)
```

But checking the library function calls using ltrace, it will show the incorrect function for the obfuscated version:
```
$ ltrace ./example/example
memcmp(0x7ffdd56d4739, 0x7ffdd56d4736, 3, 0x7ffdd56d4736)                                                                                          = 66
printf("return value of memcmp(buf1, buf"..., 3, 0x42, "memcmp"return value of memcmp(buf1, buf2, 3):  42 (actual function called: memcmp)
)                                                                                   = 76
strncmp("A", "A", 3)                                                                                                                               = 0
printf("return value of strncmp(buf1, bu"..., 3, 0, "strncmp"return value of strncmp(buf1, buf2, 3): 00 (actual function called: strncmp)
)                                                                                     = 77
memcmp(0x7ffdd56d4739, 0x7ffdd56d4736, 3, 0x7ffdd56d4736)                                                                                          = 66
printf("return value of memcmp(buf1, buf"..., 3, 0x42, "memcmp"return value of memcmp(buf1, buf2, 3):  42 (actual function called: memcmp)
)                                                                                   = 76
strncmp("A", "A", 3)                                                                                                                               = 0
printf("return value of strncmp(buf1, bu"..., 3, 0, "strncmp"return value of strncmp(buf1, buf2, 3): 00 (actual function called: strncmp)
)          

$ ltrace ./example/example.plt-got.rela-info.0-swap.obf 
strncmp("A", "A", 3)                                                    = 66
printf("return value of memcmp(buf1, buf"..., 3, 0x42, "memcmp"return value of memcmp(buf1, buf2, 3):  42 (actual function called: memcmp)
)        = 76
memcmp(0x7ffd0c372ac9, 0x7ffd0c372ac6, 3, 0x7ffd0c372ac6)               = 0
printf("return value of strncmp(buf1, bu"..., 3, 0, "strncmp"return value of strncmp(buf1, buf2, 3): 00 (actual function called: strncmp)
)          = 77
strncmp("A", "A", 3)                                                    = 66
printf("return value of memcmp(buf1, buf"..., 3, 0x42, "memcmp"return value of memcmp(buf1, buf2, 3):  42 (actual function called: memcmp)
)        = 76
memcmp(0x7ffd0c372ac9, 0x7ffd0c372ac6, 3, 0x7ffd0c372ac6)               = 0
printf("return value of strncmp(buf1, bu"..., 3, 0, "strncmp"return value of strncmp(buf1, buf2, 3): 00 (actual function called: strncmp)
)          = 77
+++ exited (status 0) +++
```

Another confusing example is when the memcmp's rela-offset is copied to strncmp:
```
$ ./pltfall example/example example/example.rela-offset.1-copy.plt; mv example/example.obf example/example.rela-offset.1-copy.obf
aoeuidhtns...

$ ./example/example.rela-offset.1-copy.obf
return value of memcmp(buf1, buf2, 3):  42 (actual function called: memcmp)
return value of strncmp(buf1, buf2, 3): 00 (actual function called: strncmp)
return value of memcmp(buf1, buf2, 3):  00 (actual function called: strncmp)
return value of strncmp(buf1, buf2, 3): 00 (actual function called: strncmp)

$ ltrace ./example/example.rela-offset.1-copy.obf
memcmp(0x7ffc812606a9, 0x7ffc812606a6, 3, 0x7ffc812606a6)               = 66
printf("return value of memcmp(buf1, buf"..., 3, 0x42, "memcmp"return value of memcmp(buf1, buf2, 3):  42 (actual function called: memcmp)
)        = 76
strncmp("A", "A", 3)                                                    = 0
printf("return value of strncmp(buf1, bu"..., 3, 0, "strncmp"return value of strncmp(buf1, buf2, 3): 00 (actual function called: strncmp)
)          = 77
memcmp(0x7ffc812606a9, 0x7ffc812606a6, 3, 0x7ffc812606a6)               = 0
printf("return value of memcmp(buf1, buf"..., 3, 0, "strncmp"return value of memcmp(buf1, buf2, 3):  00 (actual function called: strncmp)
)          = 77
strncmp("A", "A", 3)                                                    = 0
printf("return value of strncmp(buf1, bu"..., 3, 0, "strncmp"return value of strncmp(buf1, buf2, 3): 00 (actual function called: strncmp)
)          = 77
```

## How this works

In a normal application, a library function call works in the following steps:

1. The application calls a library function (strncmp in this example), it jumps to PLT stub of the function:
```
      ╎╎╎   ;-- strncmp:                                                                                             
      ╎╎╎   0x00401030      jmp   qword [reloc.strncmp]                ; [0x404018:8]=0x401036 ; "6\x10@"            
      ╎╎╎   0x00401036      push  0                                                                                  
      └───< 0x0040103b      jmp   section..plt 
```

2. For the first time the function is being called, the address in GOT (`[reloc.strncmp]`)
for the function is pointing at push instruction (`0x00401036`).

3. The value of the push instruction corresponds to the index in .rela.plt (RELA):

```
Relocation section '.rela.plt' at offset 0x538 contains 3 entries:
  Offset          Info           Type           Sym. Value    Sym. Name + Addend
000000404018  000200000007 R_X86_64_JUMP_SLO 0000000000000000 strncmp@GLIBC_2.2.5 + 0
000000404020  000300000007 R_X86_64_JUMP_SLO 0000000000000000 printf@GLIBC_2.2.5 + 0
000000404028  000400000007 R_X86_64_JUMP_SLO 0000000000000000 memcmp@GLIBC_2.2.5 + 0
```

4. After the value is pushed, it jumps to the first PLT stub:

```
            ;-- section..plt:                                                                                        
      ┌┌┌─> 0x00401020      push  qword [0x00404008]                   ; [12] -r-x section size 64 named .plt        
      ╎╎╎   0x00401026      jmp   qword [0x00404010]                   ; [0x404010:8]=0                              
      ╎╎╎   0x0040102c      nop   dword [rax]  
```

These instructions pushes some magic stuff and calls a function in loader that handles the function resolving.
When the resolving is done, the function address is stored to the address (GOT)
specified in .rela.plt section's offset `0x404018` (aka `reloc.strncmp`). And then the function is called.

5. Next time the library function is called, the first jmp instruction
in the PLT stub will directly jump to the function since the GOT value has been updated.

To understand better how PLT works, read [All about Procedure Linkage Table](https://maskray.me/blog/2021-09-19-all-about-procedure-linkage-table).

Most tools seem to detect the library function name by calculating the index a PLT stub corresponds to
(each PLT stub has the same size), and then finds the symbol name via RELA using that index.
This assumption breaks when values in the PLT/GOT/RELA gets modified, it messes up how the PLT
works and leads into resolving wrong function and tools showing incorrect function.

## Limitation

The library function that a PLT/GOT/RELA value is copied from must exist in the application, i.e. you cannot obfuscate a library function calls as a function that the application does not use.
pltfall relies on PLT and lazy binding, and it will behave in different way when lazy binding is not used (e.g. `LD_BIND_NOW=1`, `-z now`).

## Build

Run `make` to build the program. If you want to build the examples as well, run `make examples`.

## Usage

Run the program in the following way:

`pltfall <elf-file> <func-list-file>`

Where `elf-file` is the application to obfuscate library functions in, and `func-list-file` is the file that specifies which library functions to copy PLT/GOT/RELA value from another library function.

## Format of func-list-file

Each line in the func-list-file should be in the following structure:

`<target-function>,<source-function>,<copy|swap>,<method,...>`

 - `target-function`

Target library function to modify.

 - `source-function`

Source library function to copy value from.

 - `copy|swap`
   * `copy`: copy `method` value from `source-function` to `target-function`
   * `swap`: swap `method` value between `target-function` and `source-function`

 - `method`
   * `plt-got`: modify the address to GOT in .plt (or .plt.sec if it exists) section
   * `got`: modify the GOT address in .got section
   * `plt-push`: modify the push value in .plt section
   * `rela-offset`: modify the Elf64_Rela.r_offset value in .rela.plt section
   * `rela-info`: modify the Elf64_Rela.r_info vlaue in .rela.plt section

Multiple methods can be specified at the same time. Check `test.txt` to see how the methods affects the application.

### Example

`strncmp,memcmp,swap,plt-got,rela-info`

Swap the plt-got/rela-info value of strncmp and memcmp.

### Recommended combination of methods

#### `plt-got,rela-info`

Makes all target/source function call look like source/target. But it can easily be spotted by checking the PLT stub:
```
        ╎   ;-- strncmp:
        ╎   0x00401050      jmp   qword [reloc.memcmp]                 ; [0x404018:8]=0x401036 ; "6\x10@"
        ╎   0x00401056      push  2                                    ; 2
```

#### `plt-got,plt-push`

If you do not mind that the call to the either of the library function stops working properly, you can use this combination.
This can be tricky to spot unless the user knows how the push value should look like in the PLT stub.

## Detecting pltfall modified binary

Checking whether a binary has been modified using pltfall is pretty simple using some tools.

### `plt-got`

Run `objdump -d <binary>` and check GOT address in function PLT stub.
If it is not modified, the GOT address should be in ascending order.

### `got`

Use rizin or something and check the addresses in .got.plt section.
If it is not modified, the addresses should be in ascending order (except null pointers at the beginning).

### `plt-push`

Run `objdump -d <binary>` and check push instructions in PLT stub.
If it is not modified, the push values should be in ascending order starting from zero and increasing by one for each function.


### `rela-offset`

Run `readelf -r <binary>` and check the offset value in .rela.plt relocations.
If it is not modified, the offset should be in ascending order.

### `rela-info`

Run `readelf -r <binary>` and check the first four hex in info value in .rela.plt relocations.
If it is not modified, the value should be in ascending order.
