# AddressSanitizer 
## Brief for CS61


AddressSanitizer(ASan) was written by [Konstantin Serebryany](https://ai.google/research/people/KonstantinSerebryany/), [Derek Bruening](https://ai.google/research/people/author58045/), [Alexander Potapenko](https://ai.google/research/people/AlexanderPotapenko/), [Dmitry Vyukov](https://ai.google/research/people/DmitryVyukov/) at Google. It was formally presented in the paper [AddressSanitizer: A Fast Address Sanity Checker](https://ai.google/research/pubs/pub37752) in 2012.

## What ASan Does
ASan is a module for many popular compilers including GCC and Clang. It finds spatial bugs such as [buffer overflows](https://cs61.seas.harvard.edu/site/2018/Asm4/) within the stack, the heap, and globally, as well temporal bugs like use after free errors within a user's application. These are also commonly referred to as addressability issues within the context of ASan. 

## How ASan Works

ASan works in the following way:

It intercepts memory access calls at the end of the compilation pipeline using its custom versions of `malloc` and `free`. The ASan version of `malloc` builds a version of the user's application memory which is called the Shadow Memory.  ASan's `free` implementation puts deallocated objects into what is called the quarantine in order to delay their use and detect use after free bugs. 

The redzone that is added by ASan's `malloc` acts as a buffer around the application address. The larger the buffer, the larger the overflow which can be detected. For every `n` regions in memory there are `n+1` redzones. These are similar to our chunks of metadata used in Pset 1 Dmalloc. 

The shadow memory is a many to one mapping of the application memory. It maps every 8 bytes to 1 byte of metadata. These addresses contain metadata similar in fashion to what we did for the Dmalloc pset. An example of how shadow memory is encoded for x86_64 is below: 
```
Shadow = (Mem >> 3) + 0x7fff8000;
```

After encoding, these addresses are laid out in sequential order as in an array so that the header redzone acts as the near boundary for an address and the header of the next address acts as its footer.
```
|redzone1|mem1|redzone2|mem2|redzone3|mem3|
```
 Both redzones and the freed quarantine are "poisoned". Any attempts to access poisoned areas will result in an error from ASan. 


## Known limitations: 

One important thing to note about ASan is that it works on aligned memory addresses. Unaligned addresses cause issues. Additionally, any attempts to access memory that jumps over buffer sizes will be undetected by ASan. These are called non-linear buffer overflows. Additionally, ASan does not currently instrument assembly code, detect bugs in pre-compiled binaries, or detect when the kernel accesses invalid user-space memory. 

As the ASan team from Google notes:
>ASAN is not a security hardening tool: the redzones and quarantine are easy to bypass.


## Sources
1. [ASan Github](https://github.com/google/sanitizers)
1. [AddressSanitizer: A Fast Address Sanity Checker](https://ai.google/research/pubs/pub37752)
1. [LLVM Source](https://github.com/llvm/)
1. [Memory Tagging and how it improves C/C++ memory safety
](https://arxiv.org/abs/1802.09517)
