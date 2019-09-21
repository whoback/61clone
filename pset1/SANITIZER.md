# AddressSanitizer 
## Brief for CS61


AddressSanitizer(ASan) was written by [Konstantin Serebryany](https://ai.google/research/people/KonstantinSerebryany/), [Derek Bruening](https://ai.google/research/people/author58045/), [Alexander Potapenko](https://ai.google/research/people/AlexanderPotapenko/), [Dmitry Vyukov](https://ai.google/research/people/DmitryVyukov/) at Google. It was formally presented in the paper [AddressSanitizer: A Fast Address Sanity Checker](https://ai.google/research/pubs/pub37752) in 2012.

## What ASan Does
ASan finds buffer overflows within the stack, the heap, and globally, as well as use after free errors within the execution of a user's application. These are also commonly referred to as addressability issues within the context of ASan. 

## How ASan Works
Definitions
- Shadow Memory: a mapping of the user application address space to a single address
- Shadow Mapping:
- Shadow Encoding:
- Redzone:
- Poison:


ASan, which is integrated into GCC and Clang compiliers (plus others) works in the following way:
1. Provides full malloc replacement which
    1. encodes this memory into a 'state' 1 - 9 as a byte
    2. checks this byte 
    3. Initializes shadow memory at startup
● 
○ Insert poisoned redzones around allocated memory
○ Quarantine for free-ed memory
○ Collect stack traces for every malloc/free
● Provides interceptors for functions like strlen
● Prints error messages


divides program memory into 3 spaces
regular program memory
shadow memory where ASan works
untouchable spaces

basically build a shadow memory based on the application memory using
a mapping
mapping is essentially the address of the applicaion memory divided by 8 (right bit shift 3)
plus some constant

redzones for stack "poision"



## Sources
https://github.com/google/sanitizers
https://ai.google/research/pubs/pub37752
